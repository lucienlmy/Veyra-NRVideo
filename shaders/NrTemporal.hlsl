// Adapted from SAOG0721/Magpie, GPL-3.0, commit
// 3841698348bfb246623d4acf791984c8b68a577b
// src/Magpie.Core/DLSSNRTemporalShader.h (motion route 2).
// Veyra: D3D12 Load-based bilinear sampling, source-flow extent conversion,
// linear signed HDR guide, reset via zero weight, post-protection residual.
#include "NrProtection.hlsli"
Texture2D<float4> Base:register(t0);
Texture2D<float4> Raw:register(t1);
Texture2D<float4> History:register(t2);
Texture2D<float4> PreviousGuide:register(t3);
Texture2D<float2> Motion:register(t4);
RWTexture2D<float4> Output:register(u0);
RWTexture2D<float4> NextHistory:register(u1);
RWTexture2D<float4> NextGuide:register(u2);
cbuffer Settings:register(b0){uint2 Size;float HistoryWeight;float Total;float ProtectionEnabled;float Feather;float2 reserved;float4 Regions[4];}
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
groupshared float4 TileResidual[100];
groupshared float4 TilePosition[100];
groupshared float TileError[100];
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID,uint3 local:SV_GroupThreadID,uint lane:SV_GroupIndex){
    // The 8x8 output tile shares a one-pixel halo. Each observation is computed
    // once instead of nine times; thresholds and accumulation order stay intact.
    if(HistoryWeight>0){
        for(uint tap=lane;tap<100;tap+=64){
            int2 n=int2(id.xy)-int2(local.xy)+int2(tap%10,tap/10)-1;
            float2 oldPosition;float4 residual=0,position=0;float error=0;
            if(PreviousPosition(n,oldPosition)){
                float4 inputN=Base[n],old=Bilinear(PreviousGuide,oldPosition);
                float3 residualN=Raw[n].rgb-inputN.rgb;
                bool valid=all(isfinite(inputN))&&all(isfinite(old))&&old.a>=.999&&all(isfinite(residualN));
                float3 delta=abs(Guide(inputN.rgb)-old.rgb);
                error=max(delta.r,max(delta.g,delta.b));
                residual=float4(residualN,valid?1:0);
                position=float4(oldPosition,1,0);
            }
            TileResidual[tap]=residual;TilePosition[tap]=position;TileError[tap]=error;
        }
    }
    GroupMemoryBarrierWithGroupSync();
    int2 p=id.xy;if(any(id.xy>=Size))return;
    float4 base=Base[p],raw=Raw[p];bool finite=all(isfinite(base))&&all(isfinite(raw));
    NextGuide[p]=all(isfinite(base))?float4(Guide(base.rgb),1):0;
    if(!finite){NextHistory[p]=0;Output[p]=all(isfinite(base))?base:float4(0,0,0,1);return;}
    float3 current=raw.rgb-base.rgb,result=current;float2 previous;
    // A zero correction is a valid observation, not a protection mask. Keep
    // protected/feathered pixels exact and exclude them from future history.
    bool protectedPixel=Total==0||NrProtection(float2(p)+.5,Size,ProtectionEnabled,Feather,Regions)>0;
    if(protectedPixel){NextHistory[p]=0;Output[p]=raw;return;}
    uint center=(local.y+1)*10+local.x+1;
    if(HistoryWeight>0&&TilePosition[center].z>0){
        previous=TilePosition[center].xy;
        float error=0,maximumError=0;bool valid=true;float3 lo=current,hi=current;
        [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x){
            int2 n=p+int2(x,y);uint tap=center+y*10+x;
            float2 previousN=TilePosition[tap].xy;
            if(TilePosition[tap].z==0){valid=false;continue;}
            if(any(abs((previousN-n)-(previous-p))>2)){valid=false;continue;}
            float3 residualN=TileResidual[tap].rgb;
            if(TileResidual[tap].a==0){valid=false;continue;}
            float e=TileError[tap];error+=e/9;maximumError=max(maximumError,e);lo=min(lo,residualN);hi=max(hi,residualN);
        }
        float q=(1-smoothstep(.008,.04,error))*(1-smoothstep(.025,.10,maximumError));
        if(valid&&q>0){float4 old=Bilinear(History,previous);if(all(isfinite(old))&&old.a>=.999){float3 margin=.02+q*abs(old.rgb);result=lerp(current,clamp(old.rgb,lo-margin,hi+margin),HistoryWeight*q);}}
    }
    NextHistory[p]=float4(clamp(result,-65504,65504),1);
    // Preserve signed wide-gamut/HDR working components (no SDR saturate).
    Output[p]=float4(clamp(base.rgb+result,-65504,65504),raw.a);
}
