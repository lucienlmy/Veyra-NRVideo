#pragma once
#include <cstdint>
#include <optional>

namespace veyra::engine {
// A failed display query is not evidence of an HDR/SDR transition. Start in
// SDR until the first successful query, then retain the last known state.
class HdrDisplayState {
public:
    bool update(std::uintptr_t target,std::optional<bool> active) {
        if(target!=target_){target_=target;active_=false;}
        if(active)active_=*active;
        return active_;
    }
private:
    std::uintptr_t target_=0;
    bool active_=false;
};
}
