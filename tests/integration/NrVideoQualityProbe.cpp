#include "veyra/pipeline/EnhanceGraph.h"
#include "veyra/source/MediaFileSource.h"
#include "veyra/gfx/D3D12DeviceContext.h"
#include "veyra/gfx/CommandSlotRing.h"
#include "veyra/RuntimePaths.h"
#include "veyra/Log.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <array>
#include <string_view>
#include <d3d12sdklayers.h>
#include <vector>

// Offline, matched natural-frame observations. Readbacks and CPU waits are
// confined to this executable; these timings are NOT playback performance.
namespace {
using namespace veyra;
constexpr unsigned cropWidth=640,cropHeight=360;
bool dumpCrop(gfx::D3D12DeviceContext& ctx,gfx::CommandSlotRing& ring,
              ID3D12Resource* texture,std::ofstream& output){
    if(!texture)return false;
    const auto desc=texture->GetDesc();
    const unsigned channels=desc.Format==DXGI_FORMAT_R16G16_FLOAT?2:4;
    if((desc.Format!=DXGI_FORMAT_R16G16_FLOAT&&desc.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT)||
       desc.Width<cropWidth||desc.Height<cropHeight)return false;
    const unsigned rowBytes=cropWidth*channels*2;
    const unsigned pitch=(rowBytes+255)&~255u;
    const UINT64 bytes=UINT64(pitch)*cropHeight;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;
    buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    if(FAILED(ctx.device()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,
        D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback))))return false;
    Status status;unsigned slot;auto* list=ring.acquireNext(slot,status);if(!list)return false;
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={texture,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE};
    list->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=texture;
    src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.pResource=readback.Get();
    dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Footprint={desc.Format,cropWidth,cropHeight,1,pitch};
    const unsigned x=(unsigned(desc.Width)-cropWidth)/2,y=(desc.Height-cropHeight)/2;
    const D3D12_BOX box{x,y,0,x+cropWidth,y+cropHeight,1};
    list->CopyTextureRegion(&dst,0,0,0,&src,&box);
    std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);
    if(!ring.submitAndSignal(slot)||!ring.waitIdle())return false;
    void* data=nullptr;D3D12_RANGE range{0,SIZE_T(bytes)};
    if(FAILED(readback->Map(0,&range,&data)))return false;
    for(unsigned row=0;row<cropHeight;++row)
        output.write(static_cast<const char*>(data)+size_t(row)*pitch,rowBytes);
    D3D12_RANGE empty{};readback->Unmap(0,&empty);return bool(output);
}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=5&&argc!=6)return 2; // source, output directory, start seconds, frame count, optional video-sr4k
    const bool videoSr4k=argc==6&&std::wstring_view(argv[5])==L"video-sr4k";
    if(argc==6&&!videoSr4k)return 2;
    const double start=_wtof(argv[3]);const unsigned count=unsigned(_wtoi(argv[4]));
    if(start<0||count<2||count>240)return 2;
    const std::filesystem::path directory=argv[2];std::filesystem::create_directories(directory);
    Logger::instance().openFile((directory/L"engine.log").wstring());
    Logger::instance().setConsoleEnabled(false);
    gfx::D3D12DeviceContext ctx;gfx::CommandSlotRing ring;Status status;
    gfx::DeviceContextDesc device;device.enableDebugLayer=true;
    if(!ctx.initialize(device,status)||!ring.initialize(ctx.device(),ctx.directQueue(),ctx.fence(),ctx.fenceEvent(),6,status))return 2;
    source::MediaFileSource source;source::SourceOpenDesc open;
    open.path=argv[1];open.preferHardwareDecode=false;
    if(!source.open(open))return 2;
    const auto info=source.info();
    // Keep input/motion at 1080p; the optional SR case exercises a 4K working image.
    if(info.width!=1920||info.height!=1080)return 2;
    if(start>0&&!source.seek({int64_t(start*1000),1000}))return 2;
    pipeline::EnhanceGraph graph(ctx,ring);pipeline::EnhanceGraphDesc settings;
    settings.sourceWidth=settings.workWidth=settings.nrWidth=settings.flowWidth=info.width;
    settings.sourceHeight=settings.workHeight=settings.nrHeight=settings.flowHeight=info.height;
    settings.enableNr=true;settings.nrTemporal=true;settings.enableFg=false;settings.enableSr=false;
    settings.enableNvofStandalone=true;settings.runtimeAbsPath=runtime::localRuntimeDirectory().wstring();
    if(videoSr4k){
        settings.workWidth=3840;settings.workHeight=2160;
        settings.enableSr=true;settings.videoSrQuality=3;
    }
    if(!graph.initialize(settings)||!graph.createViews())return 2;
    std::array<std::ofstream,4> streams;
    const char* names[]={"base.rgba16f","raw.rgba16f","filtered.rgba16f","motion.rg16f"};
    for(unsigned i=0;i<4;++i)streams[i].open(directory/names[i],std::ios::binary);
    std::ofstream metadata(directory/L"frames.csv");metadata<<"index,source,ptsMs,epoch\n";
    bool ok=true;unsigned captured=0;
    while(captured<count){
        pipeline::FramePacket packet;const AVFrame* frame=nullptr;
        if(source.read(packet,&frame)!=source::SourceReadStatus::Frame){ok=false;break;}
        if(packet.pts.toDouble()<start)continue;
        pipeline::EnhanceGraph::FrameOutputs output;
        if(!graph.process(frame,packet.pts.toDouble()*1000,captured==0||(videoSr4k&&captured==count/2),output,packet.sequence,&packet.colorInfo)||!ring.waitIdle()){ok=false;break;}
        ID3D12Resource* textures[]={graph.diagnosticNrBase(),graph.diagnosticNrRaw(),graph.diagnosticNrFiltered(),graph.flowResource()};
        for(unsigned i=0;i<4;++i)ok=dumpCrop(ctx,ring,textures[i],streams[i])&&ok;
        if(!ok)break;
        metadata<<captured<<','<<packet.sequence<<','<<packet.pts.toDouble()*1000<<','<<output.batch.identity.epoch<<'\n';
        ++captured;
    }
    ring.drainQueue();graph.shutdown();source.close();
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> debug;
    unsigned errors=0;
    if(FAILED(ctx.device()->QueryInterface(IID_PPV_ARGS(&debug))))return 2;
    for(UINT64 i=0;i<debug->GetNumStoredMessages();++i){
        SIZE_T size=0;if(FAILED(debug->GetMessage(i,nullptr,&size)))return 2;
        std::vector<unsigned char> bytes(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
        if(FAILED(debug->GetMessage(i,message,&size)))return 2;
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){++errors;std::cerr<<message->pDescription<<'\n';}
    }
    std::cout<<"captured="<<captured<<" requested="<<count<<" crop=640x360 centered; linear FP16; real NR/NVOF, offline only\n";
    std::cout<<"debugErrors="<<errors<<'\n';
    return ok&&captured==count&&metadata&&errors==0?0:1;
}
