#pragma once
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace maobs {
struct LiveAudioItem {
    std::string source,scene;
    float weight=0;
    bool controlled=false;
    std::string mode="full";
};
class LiveAudioPolicy {
    std::set<std::string> ready;
    std::string target;
public:
    void clear() {ready.clear();target.clear();}
    std::map<std::string,float> gains(const std::vector<LiveAudioItem> &items,
        const std::string &next_target,bool reached) {
        if (target!=next_target) {target=next_target;ready.erase(target);}
        if (reached) ready.insert(target);
        std::set<std::string> present,controlled;
        for (const auto &item:items) {
            present.insert(item.scene);
            // A fully visible old scene also covers a completed fade between
            // UI polls. An interrupted, partly visible scene stays unready.
            if (item.scene!=target && item.weight>=.999f) ready.insert(item.scene);
            if (item.controlled) controlled.insert(item.source);
        }
        for (auto it=ready.begin();it!=ready.end();)
            if (!present.count(*it)) it=ready.erase(it); else ++it;
        std::map<std::string,float> result;
        for (const auto &item:items) {
            if (!controlled.count(item.source)) continue;
            const float weight=std::clamp(item.weight,0.f,1.f);
            float gain=weight; // native contribution of an uncontrolled use
            if (item.controlled) {
                if (item.mode=="mute" || weight<=0.f) gain=0.f;
                else if (item.mode=="fade") gain=weight;
                else if (item.mode=="end") gain=ready.count(item.scene)?1.f:0.f;
                else gain=1.f;
            }
            // One physical input can appear in several visible scenes. Its
            // audio is mixed once, with combined contributions capped at unity.
            result[item.source]=std::min(1.f,result[item.source]+gain);
        }
        return result;
    }
};
}
