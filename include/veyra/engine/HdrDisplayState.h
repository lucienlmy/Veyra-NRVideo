#pragma once
#include <optional>

namespace veyra::engine {
// A failed display query is not evidence of an HDR/SDR transition. Start in
// SDR until the first successful query, then retain the last known state.
class HdrDisplayState {
public:
    bool update(std::optional<bool> active) {
        if(active)active_=*active;
        return active_;
    }
private:
    bool active_=false;
};
}
