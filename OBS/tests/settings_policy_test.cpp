#include "../src/settings_policy.h"
#include <cassert>
using maobs::RuleSource;
int main() {
    for(auto source:{RuleSource::None,RuleSource::FFmpeg,RuleSource::VLC,RuleSource::LiveAudio}) {
        for(const char *start:{"begin","end","percent","delay","before_end","manual"}) {
            for(bool prepared:{false,true}) {
                const auto off=maobs::rule_availability(source,false,start,prepared,2000,3000);
                assert(!off.edit && !off.video && !off.percent && !off.delay && !off.expected && !off.lead);
                assert(!off.automatic_events && !off.prepare && !off.display && !off.invalid_timing);
            }
        }
    }
    const auto none=maobs::rule_availability(RuleSource::None,true,"percent",true,2000,1000);
    assert(!none.edit && !none.percent);
    const auto live=maobs::rule_availability(RuleSource::LiveAudio,true,"before_end",true,2000,3000);
    assert(live.edit && !live.video && !live.invalid_timing && !live.prepare && !live.display);
    const auto percent=maobs::rule_availability(RuleSource::FFmpeg,true,"percent",false,2000,3000);
    assert(percent.percent && !percent.delay && !percent.expected && !percent.lead && !percent.invalid_timing);
    assert(percent.prepare && !percent.display);
    const auto delay=maobs::rule_availability(RuleSource::FFmpeg,true,"delay",true,2000,3000);
    assert(delay.delay && !delay.percent && !delay.expected && !delay.invalid_timing && delay.display);
    for(const char *start:{"begin","end","manual"}) {
        const auto s=maobs::rule_availability(RuleSource::FFmpeg,true,start,true,2000,3000);
        assert(!s.percent && !s.delay && !s.expected && !s.lead && !s.invalid_timing);
        assert(s.automatic_events==(std::string(start)!="manual"));
    }
    const auto unknown=maobs::rule_availability(RuleSource::FFmpeg,true,"before_end",true,0,3000);
    assert(unknown.expected && !unknown.lead && !unknown.invalid_timing);
    const auto known=maobs::rule_availability(RuleSource::FFmpeg,true,"before_end",true,2000,1000);
    assert(known.expected && known.lead && !known.invalid_timing);
    assert(!maobs::rule_availability(RuleSource::FFmpeg,true,"before_end",true,2000,2000).invalid_timing);
    assert(maobs::rule_availability(RuleSource::FFmpeg,true,"before_end",true,2000,2001).invalid_timing);
    const auto vlc=maobs::rule_availability(RuleSource::VLC,true,"percent",true,2000,1000);
    assert(vlc.video && vlc.percent && !vlc.prepare && !vlc.display);
    // Swapping between advanced and OBS-managed presentation must not erase
    // a saved image preference; an explicit Loop selection is a real edit.
    assert(maobs::basic_end_mode("black",false)=="black");
    assert(maobs::basic_end_mode("hold",false)=="hold");
    assert(maobs::basic_end_mode("black",true)=="loop");
    assert(maobs::basic_end_mode("loop",false)=="hold");
}
