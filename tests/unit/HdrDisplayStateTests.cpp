#include "veyra/engine/HdrDisplayState.h"
#include "veyra/engine/EnhancementSettings.h"
#include <iostream>

int main(){
    using namespace veyra::engine;
    HdrDisplayState display;
    EnhancementSettings settings;
    bool ok=!display.update(1,std::nullopt);
    bool output=false;
    unsigned rebuilds=0;
    auto poll=[&](std::optional<bool> sample){
        const bool next=settings.useHdrPreview(true,display.update(1,sample));
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
    ok=ok&&!display.update(2,std::nullopt);
    ok=ok&&!display.update(2,false);
    ok=ok&&!display.update(1,std::nullopt);
    ok=ok&&display.update(1,true);
    ok=ok&&!display.update(0,std::nullopt);
    HdrDisplayState reopened;
    ok=ok&&!reopened.update(1,std::nullopt);
    std::cout<<"HDR query failures preserve output; real transitions and user SDR override apply: "<<ok<<'\n';
    return ok?0:1;
}
