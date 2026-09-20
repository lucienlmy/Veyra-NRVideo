// Adapted from SAOG0721/Magpie, GPL-3.0, commit
// 3841698348bfb246623d4acf791984c8b68a577b
// src/Magpie.Core/DLSSNRTemporalShader.h (motion route 2).
// Veyra: D3D12 Load-based bilinear sampling, source-flow extent conversion,
// linear signed HDR guide, reset via zero weight, post-protection residual.
Texture2D<float4> Base:register(t0);
Texture2D<float4> Raw:register(t1);
Texture2D<float4> History:register(t2);
Texture2D<float4> PreviousGuide:register(t3);
Texture2D<float2> Motion:register(t4);
RWTexture2D<float4> Output:register(u0);
RWTexture2D<float4> NextHistory:register(u1);
RWTexture2D<float4> NextGuide:register(u2);
cbuffer Settings:register(b0){uint2 Size;float HistoryWeight;float pad;float4 reserved;}
float3 Guide(float3 c){return c/(1+abs(c));}
bool Inside(int2 p){return all(p>=0)&&all(p<int2(Size));}
float4 Bilinear(Texture2D<float4> tex,float2 p){
    int2 a=int2(floor(p));float2 f=frac(p);int2 b=min(a+1,int2(Size)-1);
    return lerp(lerp(tex[a],tex[int2(b.x,a.y)],f.x),lerp(tex[int2(a.x,b.y)],tex[b],f.x),f.y);
}
bool PreviousPosition(int2 p,out float2 previous){
    previous=p;if(!Inside(p))return false;
    uint w,h;Motion.GetDimensions(w,h);
    float2 at=(float2(p)+.5)*float2(w,h)/Size-.5;
    int2 a=int2(floor(at));float2 f=frac(at);int2 hi=int2(w,h)-1;
    float2 mv=lerp(lerp(Motion[clamp(a,0,hi)],Motion[clamp(a+int2(1,0),0,hi)],f.x),lerp(Motion[clamp(a+int2(0,1),0,hi)],Motion[clamp(a+1,0,hi)],f.x),f.y)*float2(Size)/float2(w,h);
    if(!all(isfinite(mv)))return false;previous+=mv;
    return all(previous>=0)&&all(previous<=float2(Size)-1);
}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){
    int2 p=id.xy;if(any(id.xy>=Size))return;
    float4 base=Base[p],raw=Raw[p];bool finite=all(isfinite(base))&&all(isfinite(raw));
    NextGuide[p]=all(isfinite(base))?float4(Guide(base.rgb),1):0;
    if(!finite){NextHistory[p]=0;Output[p]=all(isfinite(base))?base:float4(0,0,0,1);return;}
    float3 current=raw.rgb-base.rgb,result=current;float2 previous;
    // Preserve exact bypass/protection pixels; never revive a masked residual.
    if(HistoryWeight>0&&any(abs(current)>1e-7)&&PreviousPosition(p,previous)){
        float error=0,maximumError=0;bool valid=true;float3 lo=current,hi=current;
        [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x){
            int2 n=p+int2(x,y);float2 previousN;
            if(!PreviousPosition(n,previousN)){valid=false;continue;}
            if(any(abs((previousN-n)-(previous-p))>2)){valid=false;continue;}
            float4 inputN=Base[n],old=Bilinear(PreviousGuide,previousN);
            float3 residualN=Raw[n].rgb-inputN.rgb;
            if(!all(isfinite(inputN))||!all(isfinite(old))||old.a<.999||!all(isfinite(residualN))){valid=false;continue;}
            float3 delta=abs(Guide(inputN.rgb)-old.rgb);float e=max(delta.r,max(delta.g,delta.b));error+=e/9;maximumError=max(maximumError,e);lo=min(lo,residualN);hi=max(hi,residualN);
        }
        float q=(1-smoothstep(.008,.04,error))*(1-smoothstep(.025,.10,maximumError));
        if(valid&&q>0){float4 old=Bilinear(History,previous);if(all(isfinite(old))&&old.a>=.999){float3 margin=.02+q*abs(old.rgb);result=lerp(current,clamp(old.rgb,lo-margin,hi+margin),HistoryWeight*q);}}
    }
    NextHistory[p]=float4(clamp(result,-65504,65504),1);
    // Preserve signed wide-gamut/HDR working components (no SDR saturate).
    Output[p]=float4(clamp(base.rgb+result,-65504,65504),raw.a);
}
