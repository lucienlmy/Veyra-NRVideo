#pragma once
#include "veyra/pipeline/GpuPassUtils.h"
#include "veyra/engine/EnhancementSettings.h"
#include <array>

namespace veyra::pipeline {
// Residual history stays on the graph's ordered GPU queue. Descriptor sets are
// immutable; neither CPU waits nor descriptor rewrites occur per frame.
class NrTemporalPass {
public:
    bool initialize(ID3D12Device*,ID3D12Resource* base,ID3D12Resource* motion,ID3D12Resource* output);
    ID3D12Resource* raw()const{return raw_.Get();}
    void run(ID3D12GraphicsCommandList*,StateTracker&,bool reset,bool haveMotion,double frameMs,
             float total,const engine::ProtectionSettings& protection);
    void reset(){valid_=false;}
    void close();
private:
    ComputePass pass_;
    ComPtr<ID3D12Resource> raw_,history_[2],guide_[2];
    ID3D12Resource* base_=nullptr;ID3D12Resource* motion_=nullptr;ID3D12Resource* output_=nullptr;
    unsigned width_=0,height_=0,index_=0;
    bool valid_=false;
};
}
