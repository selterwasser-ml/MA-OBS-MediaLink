#pragma once
#include <string>

namespace maobs {
enum class RuleSource { None, FFmpeg, VLC, LiveAudio };
inline std::string basic_end_mode(const std::string &stored,bool loop_selected) {
    if (loop_selected) return "loop";
    // Passive capability changes preserve hold/black; only explicitly leaving
    // Loop needs a non-loop value in the existing, backwards-compatible schema.
    return stored=="loop"?"hold":stored;
}
struct RuleAvailability {
    bool edit=false,video=false,percent=false,delay=false,expected=false,lead=false;
    bool automatic_events=false,prepare=false,display=false;
    bool invalid_timing=false;
};
inline RuleAvailability rule_availability(RuleSource source,bool enabled,
    const std::string &start,bool prepare_source,int expected_ms,int lead_ms) {
    RuleAvailability s;
    s.edit=source!=RuleSource::None && enabled;
    s.video=s.edit && source!=RuleSource::LiveAudio;
    s.percent=s.video && start=="percent";
    s.delay=s.video && start=="delay";
    s.expected=s.video && start=="before_end";
    s.lead=s.expected && expected_ms>0;
    s.automatic_events=s.video && start!="manual";
    s.prepare=s.video && source==RuleSource::FFmpeg;
    s.display=s.prepare && prepare_source;
    s.invalid_timing=s.lead && lead_ms>expected_ms;
    return s;
}
}
