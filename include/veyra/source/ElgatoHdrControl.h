#pragma once
#include "veyra/pipeline/FramePacket.h"
#include <dshow.h>
#include <wrl/client.h>
#include <array>
#include <string_view>

namespace veyra::source {
struct ElgatoHdrPacket {
    bool valid=false;
    unsigned eotf=0;
    float peak=0, maxCll=0, maxFall=0;
};
bool isElgatoMk2(std::wstring_view name);
ElgatoHdrPacket parseElgatoHdrPacket(const std::array<unsigned char,32>& bytes);

// Session-scoped device control; release it after Stop and before the filter.
class ElgatoHdrControl {
public:
    ~ElgatoHdrControl();
    ElgatoHdrControl()=default;
    ElgatoHdrControl(const ElgatoHdrControl&)=delete;
    ElgatoHdrControl& operator=(const ElgatoHdrControl&)=delete;
    bool configure(IUnknown* filter,std::wstring_view name,bool p010,
                   unsigned overrideValue,pipeline::ColorDescription& color);
private:
    Microsoft::WRL::ComPtr<IKsPropertySet> properties_;
    DWORD original_=0;
    bool restore_=false;
};
}
