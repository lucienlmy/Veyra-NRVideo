#include "veyra/pipeline/NrTemporalPass.h"
#include <bit>
#include <cmath>
#include <algorithm>
namespace veyra::pipeline {
bool NrTemporalPass::initialize(ID3D12Device* device,ID3D12Resource* base,ID3D12Resource* motion,ID3D12Resource* output){
    close();base_=base;motion_=motion;output_=output;
    if(!base||!motion||!output)return false;
    auto desc=base->GetDesc();width_=unsigned(desc.Width);height_=desc.Height;
    raw_=makeTexture(device,width_,height_,DXGI_FORMAT_R16G16B16A16_FLOAT,true);
    for(unsigned i=0;i<2;++i){history_[i]=makeTexture(device,width_,height_,DXGI_FORMAT_R16G16B16A16_FLOAT,true);guide_[i]=makeTexture(device,width_,height_,DXGI_FORMAT_R16G16B16A16_FLOAT,true);if(!history_[i]||!guide_[i])return false;}
    std::vector<uint8_t> shader;if(!raw_||!pass_.loadShader("NrTemporal.dxil",shader)||!pass_.create(device,shader,16,5,3,8))return false;
    DescriptorStager staging;if(!staging.initialize(device,10))return false;
    for(unsigned i=0;i<2;++i){
        ID3D12Resource* sources[]={base,raw_.Get(),history_[1-i].Get(),guide_[1-i].Get(),motion};
        for(unsigned j=0;j<5;++j)staging.stageSrv(sources[j],nullptr,pass_.heap.Get(),i*8+j);
        makeUav(device,output,DXGI_FORMAT_R16G16B16A16_FLOAT,cpuHandleOf(pass_,i*8+5));
        makeUav(device,history_[i].Get(),DXGI_FORMAT_R16G16B16A16_FLOAT,cpuHandleOf(pass_,i*8+6));
        makeUav(device,guide_[i].Get(),DXGI_FORMAT_R16G16B16A16_FLOAT,cpuHandleOf(pass_,i*8+7));
    }return true;
}
void NrTemporalPass::run(ID3D12GraphicsCommandList* list,StateTracker& tracker,bool reset,bool haveMotion,double frameMs){
    const auto i=index_;const bool useHistory=valid_&&!reset&&haveMotion;
    tracker.transition(list,raw_.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    tracker.transition(list,base_,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    tracker.transition(list,motion_,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    tracker.transition(list,history_[1-i].Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    tracker.transition(list,guide_[1-i].Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    for(auto* r:{output_,history_[i].Get(),guide_[i].Get()})tracker.transition(list,r,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    // 80 ms past-history EMA; no future frames or presentation holdback.
    const float weight=useHistory?float(std::exp(-std::clamp(frameMs,1.0,250.0)/80.0)):0.f;
    const float c[8]={std::bit_cast<float>(width_),std::bit_cast<float>(height_),weight,0,0,0,0,0};
    pass_.bind(list,c,gpuHandleOf(pass_,i*8).ptr,gpuHandleOf(pass_,i*8+5).ptr);
    list->Dispatch((width_+7)/8,(height_+7)/8,1);
    for(auto* r:{output_,history_[i].Get(),guide_[i].Get()}){tracker.uavBarrier(list,r);tracker.transition(list,r,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}
    valid_=true;index_=1-i;
}
void NrTemporalPass::close(){pass_={};raw_.Reset();for(auto& r:history_)r.Reset();for(auto& r:guide_)r.Reset();base_=motion_=output_=nullptr;width_=height_=index_=0;valid_=false;}
}
