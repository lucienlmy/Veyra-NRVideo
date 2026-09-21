#include "veyra/engine/HdrDisplayState.h"
#include "veyra/engine/EnhancementSettings.h"
#include <iostream>

int main(){
    using namespace veyra::engine;
    HdrDisplayState display;
    EnhancementSettings settings;
    bool ok=!display.update(std::nullopt);
    bool output=false;
    unsigned rebuilds=0;
    auto poll=[&](std::optional<bool> sample){
        const bool next=settings.useHdrPreview(true,display.update(sample));
        if(next!=output)++rebuilds;
        output=next;
    };
    poll(true);ok=ok&&output&&rebuilds==1;
    for(int i=0;i<20;++i)poll(std::nullopt);
    poll(true);ok=ok&&output&&rebuilds==1;
    poll(false);ok=ok&&!output&&rebuilds==2;
    poll(std::nullopt);ok=ok&&!output&&rebuilds==2;
    poll(true);ok=ok&&output&&rebuilds==3;
    settings.forceSdrPreview=true;
    poll(std::nullopt);ok=ok&&!output&&rebuilds==4;
    settings.forceSdrPreview=false;
    poll(std::nullopt);ok=ok&&output&&rebuilds==5;
    std::cout<<"HDR query failures preserve output; real transitions and user SDR override apply: "<<ok<<'\n';
    return ok?0:1;
}
