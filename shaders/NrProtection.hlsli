#ifndef VEYRA_NR_PROTECTION
#define VEYRA_NR_PROTECTION
float NrProtection(float2 pixel, uint2 size, float enabled, float feather, float4 regions[4]) {
    float protection=0;
    if(enabled>0) {
        for(uint i=0;i<4;++i) {
            float4 r=regions[i]*float4(size,size);
            if(r.z<=r.x||r.w<=r.y)continue;
            float edge=min(min(pixel.x-r.x,r.z-pixel.x),min(pixel.y-r.y,r.w-pixel.y));
            protection=max(protection,feather>0?smoothstep(0,feather,edge):(edge>=0?1:0));
        }
    }
    return protection;
}
#endif
