#include "veyra/pipeline/NrTemporalPass.h"
#include "veyra/gfx/D3D12DeviceContext.h"
#include "veyra/gfx/CommandSlotRing.h"
#include <DirectXPackedVector.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <d3d12sdklayers.h>

int main() {
    using namespace veyra;
    using namespace pipeline;
    using namespace DirectX::PackedVector;
    gfx::D3D12DeviceContext ctx;
    gfx::CommandSlotRing ring;
    Status status=Status::Ok;
    gfx::DeviceContextDesc desc;
    desc.enableDebugLayer=true;
    if(!ctx.initialize(desc,status)||!ring.initialize(ctx.device(),ctx.directQueue(),ctx.fence(),ctx.fenceEvent(),4,status))return 2;
    constexpr unsigned width=17,height=13,pitch=256;
    auto base=makeTexture(ctx.device(),width,height,DXGI_FORMAT_R16G16B16A16_FLOAT,true);
    auto motion=makeTexture(ctx.device(),width,height,DXGI_FORMAT_R16G16_FLOAT,true);
    auto output=makeTexture(ctx.device(),width,height,DXGI_FORMAT_R16G16B16A16_FLOAT,true);
    auto upload=makeUploadBuffer(ctx.device(),pitch*height);
    NrTemporalPass pass;
    if(!upload||!pass.initialize(ctx.device(),base.Get(),motion.Get(),output.Get()))return 2;
    D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bd{};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=pitch*height;bd.Height=1;bd.DepthOrArraySize=1;bd.MipLevels=1;bd.SampleDesc.Count=1;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    if(FAILED(ctx.device()->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback))))return 2;
    StateTracker states;
    bool patterned=false;
    auto fill=[&](ID3D12Resource* texture,float value,bool flow=false){
        void* data=nullptr;if(FAILED(upload->Map(0,nullptr,&data)))return false;
        auto* pixels=static_cast<HALF*>(data);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)for(unsigned c=0;c<(flow?2u:4u);++c)
            pixels[y*pitch/2+x*(flow?2:4)+c]=XMConvertFloatToHalf(c==3?1.f:value+(patterned&&!flow?float((x*13+y*7+c*3)%19)*.013f:0.f));
        upload->Unmap(0,nullptr);
        unsigned slot;auto* list=ring.acquireNext(slot,status);if(!list)return false;
        states.transition(list,texture,D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION dst{},src{};dst.pResource=texture;dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Footprint={flow?DXGI_FORMAT_R16G16_FLOAT:DXGI_FORMAT_R16G16B16A16_FLOAT,width,height,1,pitch};
        list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
        return ring.submitAndSignal(slot)&&ring.waitIdle();
    };
    engine::ProtectionSettings protection;
    auto run=[&](float raw,bool reset,double ms=16.6667,bool haveMotion=true,float total=1.f){
        if(!fill(pass.raw(),raw))return std::numeric_limits<float>::quiet_NaN();
        unsigned slot;auto* list=ring.acquireNext(slot,status);
        pass.run(list,states,reset,haveMotion,ms,total,protection);
        states.transition(list,output.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION dst{},src{};src.pResource=output.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint.Footprint={DXGI_FORMAT_R16G16B16A16_FLOAT,width,height,1,pitch};
        list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
        if(!ring.submitAndSignal(slot)||!ring.waitIdle())return std::numeric_limits<float>::quiet_NaN();
        void* data=nullptr;D3D12_RANGE range{0,pitch*height};readback->Map(0,&range,&data);
        float result=XMConvertHalfToFloat(static_cast<HALF*>(data)[6*pitch/2+8*4]);
        uint64_t fingerprint=14695981039346656037ull;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width*4;++x){
            fingerprint^=static_cast<HALF*>(data)[y*pitch/2+x];fingerprint*=1099511628211ull;
        }
        std::cout<<"frame fingerprint="<<fingerprint<<std::endl;
        D3D12_RANGE empty{0,0};readback->Unmap(0,&empty);return result;
    };
    bool ok=fill(base.Get(),.5f)&&fill(motion.Get(),0,true);
    auto check=[&](const char* name,bool success){std::cout<<name<<"="<<success<<std::endl;ok&=success;};
    check("reset identity",run(.75f,true)==.75f);
    check("constant residual",run(.75f,false)==.75f);
    const float decay=run(.5f,false);
    check("zero residual decays",decay>.5f&&decay<.75f);
    check("no motion bypass",run(.5f,false,16,false)==.5f);
    for(double ms:{0.,-1.,251.,std::numeric_limits<double>::quiet_NaN()}){
        run(.75f,true);check("invalid interval bypass",run(.5f,false,ms)==.5f);
    }
    run(.75f,true);check("zero total bypass",run(.5f,false,16,true,0)==.5f);
    protection.enabled=true;protection.regions[0]={.3f,.3f,.7f,.7f};
    run(.75f,true);check("protected identity",run(.5f,false)==.5f);
    protection.enabled=false;
    check("protected history excluded",run(.5f,false)==.5f);
    protection.enabled=true;protection.regions[0]={.45f,.3f,.8f,.7f};protection.featherPixels=4;
    run(.75f,true);check("feather identity",run(.5f,false)==.5f);
    protection.enabled=false;
    run(.75f,true);fill(base.Get(),.1f);
    check("changed guide rejects",std::abs(run(.1f,false)-.1f)<.001f);
    fill(base.Get(),.5f);
    run(.75f,true);fill(motion.Get(),100,true);
    check("out of bounds motion bypass",run(.5f,false)==.5f);
    fill(motion.Get(),0,true);fill(base.Get(),-.5f);
    check("signed HDR identity",run(-1.f,true)==-1.f);
    fill(base.Get(),2.f);check("HDR highlight identity",run(3.f,true)==3.f);
    // Full-frame fingerprints include odd-size tile boundaries, fractional flow,
    // changing signed corrections and a mid-sequence reset for shader A/B runs.
    patterned=true;fill(base.Get(),.25f);fill(motion.Get(),.125f,true);
    for(unsigned frame=0;frame<16;++frame){
        const float value=.25f+float(int(frame%5)-2)*.04f;
        check("patterned finite",std::isfinite(run(value,frame==0||frame==9)));
    }
    ring.drainQueue();
    ComPtr<ID3D12InfoQueue> info;
    if(FAILED(ctx.device()->QueryInterface(IID_PPV_ARGS(&info))))return 2;
    unsigned errors=0;
    for(UINT64 i=0;i<info->GetNumStoredMessages();++i){
        SIZE_T size=0;if(FAILED(info->GetMessage(i,nullptr,&size)))return 2;
        std::vector<uint8_t> bytes(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
        if(FAILED(info->GetMessage(i,message,&size)))return 2;
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){++errors;std::cerr<<message->pDescription<<std::endl;}
    }
    check("debug layer clean",errors==0);
    return ok?0:1;
}
