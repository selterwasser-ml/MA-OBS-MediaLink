#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "../src/live_audio_policy.h"
static std::map<std::string,int> calls;
static bool ma_obs_exiting=false,ma_frontend_available=true,collection_changing=false,core_destroyed=false;
static uint64_t collection_epoch=1,prepared_collection_epoch=UINT64_MAX;
struct QByteArray {std::string s;const char *constData() const {return s.c_str();}};
struct QString {
    std::string s;QString()=default;QString(const char *v):s(v){}
    QByteArray toUtf8() const {return {s};}void clear(){s.clear();}
    bool operator<(const QString &o) const{return s<o.s;}
    bool operator==(const QString &o) const{return s==o.s;}
};
struct obs_source_t {int refs=1;obs_source_t *child=nullptr;};
static obs_source_t *output=nullptr,*shown=nullptr;
void obs_source_release(obs_source_t *s) {assert(!core_destroyed && s->refs>0);--s->refs;++calls["release"];}
obs_source_t *obs_get_output_source(int) {assert(!core_destroyed);if(output) ++output->refs;return output;}
void obs_set_output_source(int,obs_source_t *s) {
    assert(!core_destroyed);if(output) obs_source_release(output);output=s;if(s) ++s->refs;++calls["set_output"];
}
void obs_transition_clear(obs_source_t *s) {if(s->child){obs_source_release(s->child);s->child=nullptr;}}
obs_source_t *obs_get_source_by_uuid(const char *) {assert(!core_destroyed);if(shown) ++shown->refs;return shown;}
void obs_source_dec_showing(obs_source_t *) {assert(!core_destroyed);++calls["dec_showing"];}
struct QTimer {
    bool stopped=false;
    static std::vector<std::function<void()>> delayed;
    void stop(){stopped=true;}void disconnect(){}void deleteLater(){delete this;}
    template<class F>static void singleShot(int,F f){delayed.push_back(f);}
};
std::vector<std::function<void()>> QTimer::delayed;
struct Widget {
    std::vector<QTimer *> timers;
    template<class T>std::vector<T> findChildren(){return {timers.begin(),timers.end()};}
    void reject(){++calls["reject"];}
    void pause_for_collection_change(){++calls["pause_dock"];}
    void resume_after_collection_change(){++calls["resume_dock"];}
};
static Widget *monitor_dock=nullptr,*ma_settings_window=nullptr;
static QTimer *ma_video_runtime_timer=nullptr;
struct Pilot {bool active=false,replaced=false;obs_source_t *private_source=nullptr,*previous_output=nullptr,*program_transition=nullptr;QTimer *status_timer=nullptr;};
static Pilot program_pilot;
static std::atomic<bool> pilot_ui_active{false},pilot_ui_output_owned{false};
static bool ma_media_control_active(){return program_pilot.active&&!ma_obs_exiting;}
struct Audio {
    void clear(){assert(!core_destroyed);++calls["audio_clear"];}
    void apply(int){assert(ma_media_control_active());++calls["audio_apply"];}
};
static Audio ma_audio_gains;
void ma_restore_media_sources(){assert(!core_destroyed);++calls["restore"];}
bool ma_video_command(const QString &,const QString &) {assert(ma_media_control_active()&&!core_destroyed);++calls["command"];return true;}
int obs_source_media_get_state(obs_source_t *){assert(!core_destroyed);return 1;}
bool ma_media_can_pause(int v){return v==1;}
void ma_video_restore_hold(obs_source_t *){assert(ma_media_control_active()&&!core_destroyed);++calls["hold"];}
struct Pending {
    QString source="video";bool manual_override=false,fired=false,restore_hold_after_start=false;
    Pending(int=0){}
};
struct Video {
    bool enabled=true,suspended=false,got_event=true;
    std::string collection="test";
    maobs::LiveAudioPolicy live_audio_policy;
    std::map<QString,float> live_audio_gains;
    std::map<QString,float> media_fade_weights;
    std::map<QString,Pending> continued_audio;
    std::set<QString> forced_showing,continued_sources;
    std::map<QString,int> resume_positions;
    uint64_t last_membership_ms=1,cue_event=1;
    QString cue_scene="scene";
    std::vector<Pending> pending{1},outgoing{2};
    int last_cue=0;float last_progress=0;
    void load(){++calls["load"];}
    void tick(int){assert(ma_media_control_active());++calls["tick"];}
    void update_live_audio(int){assert(ma_media_control_active());++calls["live_audio"];}
    int audio_gains(){return 1;}
#include "stop_under_test.inc"
};
static Video ma_video;
void ma_pilot_clear_children(){assert(!core_destroyed);++calls["clear_children"];}
void ma_pilot_sync_children(){assert(ma_media_control_active());++calls["sync"];}
int ma_video_snapshot(){return 1;}
std::string ma_video_collection_name(){assert(ma_frontend_available);return "test";}
static bool auto_start=false;
bool should_autostart(){return auto_start;}
bool pilot_start(bool){assert(!ma_obs_exiting);++calls["start"];return true;}
constexpr int LOG_INFO=0,LOG_WARNING=1;
void blog(int,const char *,...){}
enum obs_frontend_event {OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN,OBS_FRONTEND_EVENT_EXIT,
    OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING,OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED,OBS_FRONTEND_EVENT_FINISHED_LOADING};
void obs_frontend_remove_event_callback(void (*)(obs_frontend_event,void *),void *) {assert(ma_frontend_available);++calls["remove_callback"];}
void obs_frontend_remove_dock(const char *) {assert(ma_frontend_available);++calls["remove_dock"];}
#include "lifecycle_under_test.inc"
static void drain() {auto callbacks=std::move(QTimer::delayed);QTimer::delayed.clear();for(auto &cb:callbacks)cb();}
int main() {
    obs_source_t original{2},media{2},wrapper{2,&media};
    output=&wrapper;program_pilot={true,false,&media,&original,&wrapper,new QTimer};
    ma_video.forced_showing.insert("video");shown=&media;
    ma_video.manual("video","restart");assert(calls["command"]==1 && ma_video.pending[0].manual_override);
    ma_media_runtime_tick();assert(calls["tick"]==1 && calls["audio_apply"]==1);
    ma_video.media_fade_weights["video"]=.3f;ma_video.continued_audio["video"]=Pending{};
    pilot_stop();
    assert(!program_pilot.active && !pilot_ui_active.load());
    assert(media.refs==0 && wrapper.refs==0 && original.refs==2);
    assert(ma_video.pending.empty() && ma_video.outgoing.empty() && !ma_video.got_event);
    assert(ma_video.media_fade_weights.empty() && ma_video.continued_audio.empty());
    assert(calls["restore"]==1 && calls["dec_showing"]==1);
    for(int i=0;i<100;++i){ma_media_runtime_tick();ma_video.manual("video","restart");}
    assert(calls["tick"]==1 && calls["audio_apply"]==1);
    assert(calls["command"]==1);
    // An ordinary inactive startup recovers exactly once, even with retries.
    schedule_collection_ready(0,collection_epoch);schedule_collection_ready(500,collection_epoch);
    int before=calls["restore"];drain();assert(calls["restore"]==before+1);
    // Early shutdown releases all objects while OBS is still usable.
    obs_source_t next_original{2},next_media{2},next_wrapper{2,&next_media};
    output=&next_wrapper;shown=&next_media;
    program_pilot={true,false,&next_media,&next_original,&next_wrapper,new QTimer};
    ma_video.pending={1};ma_video.forced_showing.insert("video");ma_video.got_event=true;
    QTimer dock_timer,dialog_timer;Widget dock{{&dock_timer}},dialog{{&dialog_timer}};
    monitor_dock=&dock;ma_settings_window=&dialog;ma_video_runtime_timer=new QTimer;
    auto_start=true;schedule_collection_ready(1500,collection_epoch);
    pilot_frontend_event(OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN,nullptr);
    assert(ma_obs_exiting && !program_pilot.active && ma_video.suspended);
    assert(next_media.refs==0 && next_wrapper.refs==0 && next_original.refs==2);
    assert(ma_video_runtime_timer==nullptr && dock_timer.stopped && dialog_timer.stopped);
    assert(calls["reject"]==1 && ma_video.pending.empty());
    core_destroyed=true; // any later OBS source API call is now a test failure
    int restored=calls["restore"],releases=calls["release"];
    drain();ma_media_runtime_tick();ma_shutdown_frontend();
    pilot_frontend_event(OBS_FRONTEND_EVENT_FINISHED_LOADING,nullptr);
    assert(calls["start"]==0 && calls["restore"]==restored && calls["release"]==releases);
    pilot_frontend_event(OBS_FRONTEND_EVENT_EXIT,nullptr);
    assert(!ma_frontend_available && calls["remove_callback"]==1);
    obs_module_unload();obs_module_unload();
    assert(calls["remove_dock"]==0 && calls["remove_callback"]==1);
    assert(calls["restore"]==restored && calls["release"]==releases);
}
