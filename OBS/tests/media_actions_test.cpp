#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
struct QByteArray {
    std::string value;
    const char *constData() const {return value.c_str();}
};
struct QString {
    std::string value;
    QString()=default;
    QString(const char *s):value(s){}
    QByteArray toUtf8() const {return {value};}
    bool operator==(const QString &other) const {return value==other.value;}
    bool operator!=(const QString &other) const {return value!=other.value;}
    bool operator<(const QString &other) const {return value<other.value;}
};
enum obs_media_state {OBS_MEDIA_STATE_NONE,OBS_MEDIA_STATE_PLAYING,OBS_MEDIA_STATE_OPENING,
    OBS_MEDIA_STATE_BUFFERING,OBS_MEDIA_STATE_PAUSED,OBS_MEDIA_STATE_STOPPED,OBS_MEDIA_STATE_ENDED,OBS_MEDIA_STATE_ERROR};
struct obs_source_t {
    obs_media_state state=OBS_MEDIA_STATE_STOPPED;
    int64_t time=42000;
    int restarts=0,plays=0,pauses=0,stops=0,refs=0,restore_hold=0;
    bool media=true;
};
static obs_source_t source;
static bool control_active=true;
bool ma_media_control_active(){return control_active;}
obs_source_t *obs_get_source_by_uuid(const char *uuid) {
    if (std::string(uuid)!="video") return nullptr;
    ++source.refs;return &source;
}
void obs_source_release(obs_source_t *s) {--s->refs;assert(s->refs>=0);}
bool ma_media_source(obs_source_t *s) {return s->media;}
bool ma_ffmpeg_source(obs_source_t *) {return false;}
obs_media_state obs_source_media_get_state(obs_source_t *s) {return s->state;}
void obs_source_media_play_pause(obs_source_t *s,bool pause) {
    if (pause) {++s->pauses;s->state=OBS_MEDIA_STATE_PAUSED;}
    else {++s->plays;s->state=OBS_MEDIA_STATE_PLAYING;}
}
void obs_source_media_restart(obs_source_t *s) {++s->restarts;s->state=OBS_MEDIA_STATE_PLAYING;s->time=0;}
void obs_source_media_stop(obs_source_t *s) {++s->stops;s->state=OBS_MEDIA_STATE_STOPPED;s->time=0;}
void obs_source_media_set_time(obs_source_t *s,int64_t value) {s->time=value;}
struct proc_handler_t {};struct calldata_t {};
proc_handler_t *obs_source_get_proc_handler(obs_source_t *) {return nullptr;}
void calldata_init(calldata_t *) {}
void calldata_free(calldata_t *) {}
void proc_handler_call(proc_handler_t *,const char *,calldata_t *) {}
void obs_source_show_preloaded_video(obs_source_t *) {}
void ma_video_restore_hold(obs_source_t *s) {++s->restore_hold;}
#include "media_command_under_test.inc"
struct Pending {
    QString source="video";
    bool fired=false,manual_override=false,restore_hold_after_start=false;
};
struct Engine {
    bool enabled=true,suspended=false;
    std::vector<Pending> pending{Pending{}};
    std::map<QString,int64_t> resume_positions;
#include "manual_under_test.inc"
};
int main() {
    Engine engine;
    // Play on a stopped/ended FFmpeg input must explicitly start the decoder.
    engine.manual("video","toggle");
    assert(source.restarts==1 && source.state==OBS_MEDIA_STATE_PLAYING);
    assert(engine.pending[0].fired && engine.pending[0].manual_override);
    source.time=17000;
    engine.manual("video","toggle");
    assert(source.pauses==1 && source.restarts==1 && source.time==17000);
    assert(!engine.pending[0].fired && engine.pending[0].manual_override);
    // Regression: repeated automatic updates must not restart after Pause.
    for (int tick=0;tick<100;++tick)
        assert(!ma_media_automatic_start_allowed(engine.pending[0].fired,engine.pending[0].manual_override,false));
    engine.manual("video","toggle");
    assert(source.plays==1 && source.restarts==1 && source.time==17000);
    engine.manual("video","stop");
    assert(source.stops==1 && source.restarts==1 && source.state==OBS_MEDIA_STATE_STOPPED);
    for (int tick=0;tick<100;++tick)
        assert(!ma_media_automatic_start_allowed(engine.pending[0].fired,engine.pending[0].manual_override,false));
    engine.pending[0].restore_hold_after_start=true;
    engine.manual("video","play");
    assert(source.restarts==2 && source.restore_hold==1 && !engine.pending[0].restore_hold_after_start);
    source.time=5000;engine.manual("video","restart");
    assert(source.restarts==3 && source.time==0);
    engine.manual("video","play");assert(source.restarts==3); // already playing: no reset
    source.state=OBS_MEDIA_STATE_ENDED;engine.manual("video","toggle");assert(source.restarts==4);
    assert(ma_media_is_playing(source.state));
    source.state=OBS_MEDIA_STATE_PAUSED;assert(!ma_media_is_playing(source.state));
    source.state=OBS_MEDIA_STATE_PLAYING;assert(ma_media_is_playing(source.state));
    source.state=OBS_MEDIA_STATE_BUFFERING;assert(!ma_media_is_playing(source.state));
    engine.manual("video","toggle");assert(source.state==OBS_MEDIA_STATE_PAUSED);
    Pending next_cue;
    assert(ma_media_automatic_start_allowed(next_cue.fired,next_cue.manual_override,false));
    assert(!ma_media_automatic_start_allowed(false,false,true)); // manual-only rule
    assert(!ma_media_automatic_start_allowed(true,false,false)); // already fired
    assert(!ma_video_command("video","invalid"));
    assert(!ma_video_command("missing","play"));
    source.media=false;assert(!ma_video_command("video","restart"));
    control_active=false;
    const int starts=source.restarts;
    engine.manual("video","restart");assert(source.restarts==starts);
    assert(!ma_video_command("video","restart"));
    assert(source.refs==0);
}
