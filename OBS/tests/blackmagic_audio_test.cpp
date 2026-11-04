#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include "../src/live_audio_policy.h"
struct QByteArray {std::string s;const char *constData() const{return s.c_str();}std::string toStdString()const{return s;}};
struct QString {
    std::string s;QString()=default;QString(const char *p):s(p){}
    QByteArray toUtf8()const{return {s};}bool isEmpty()const{return s.empty();}
    bool operator==(const QString &o)const{return s==o.s;}bool operator<(const QString &o)const{return s<o.s;}
    bool operator!=(const QString &o)const{return s!=o.s;}
};
QString ma_utf(const char *v){return QString(v?v:"");}
std::string ma_videokey(const QString &v){return v.s;}
constexpr uint32_t OBS_SOURCE_AUDIO=1;
struct obs_scene_t;
struct obs_source_t {const char *uuid,*name,*id;uint32_t flags=1;bool muted=false;obs_scene_t *scene=nullptr;int refs=0;};
struct obs_sceneitem_t {obs_source_t *source;bool visible=true;};
struct obs_scene_t {obs_source_t *owner;std::vector<obs_sceneitem_t> items;};
static std::map<std::string,obs_source_t *> registry;
const char *obs_source_get_unversioned_id(obs_source_t *s){return s->id;}
uint32_t obs_source_get_output_flags(obs_source_t *s){return s->flags;}
bool obs_source_muted(obs_source_t *s){return s->muted;}
bool obs_sceneitem_visible(obs_sceneitem_t *s){return s->visible;}
obs_source_t *obs_sceneitem_get_source(obs_sceneitem_t *s){return s->source;}
const char *obs_source_get_uuid(obs_source_t *s){return s->uuid;}
const char *obs_source_get_name(obs_source_t *s){return s->name;}
obs_scene_t *obs_group_or_scene_from_source(obs_source_t *s){return s->scene;}
obs_scene_t *obs_scene_from_source(obs_source_t *s){return s->scene;}
obs_source_t *obs_scene_get_source(obs_scene_t *s){return s->owner;}
obs_source_t *obs_get_source_by_uuid(const char *id){auto it=registry.find(id);if(it==registry.end())return nullptr;++it->second->refs;return it->second;}
void obs_source_release(obs_source_t *s){assert(--s->refs>=0);}
void obs_scene_enum_items(obs_scene_t *s,bool(*cb)(obs_scene_t *,obs_sceneitem_t *,void *),void *data){for(auto &item:s->items)if(!cb(s,&item,data))break;}
struct MAVideoEntry{QString uuid,name;bool ffmpeg=false,audio_only=false;};
#include "blackmagic_catalog_under_test.inc"
struct MALiveLayer{std::string uuid,name;float weight=0;};
struct MAVideoCue{bool valid=true;QString target_uuid="b";float progress=.5f;bool reached=false,direct=false;std::vector<MALiveLayer> live_layers;uint64_t event=1;};
struct MAVideoRule{bool enabled=false;QString audio_mode="full";QString start="begin",same_goto="ignore",on_abort="continue",on_exit="stop";};
struct MAVideoPending{QString source;MAVideoRule rule;bool fired=true;QString scene{};bool manual_override=false;};
static std::vector<std::string> media_commands;
bool ma_video_command(const QString &source,const QString &command){media_commands.push_back(source.s+":"+command.s);return true;}
int64_t obs_source_media_get_time(obs_source_t *){return 12000;}
constexpr int LOG_INFO=1;
template<class... Args> void blog(int,const char *,Args...) {}
static bool control_active=true;
bool ma_media_control_active(){return control_active;}
struct Engine{
    bool enabled=true,suspended=false;
    bool got_event=true;
    MAVideoCue last_cue;
    std::vector<MAVideoPending> pending,outgoing;
    std::set<QString> continued_sources;
    std::map<QString,MAVideoPending> continued_audio;
    std::map<QString,float> media_fade_weights;
    std::map<QString,int64_t> resume_positions;
    QString cue_scene;
    float last_progress=1;
    uint64_t cue_event=0,event_started_ms=0,last_membership_ms=0;
    maobs::LiveAudioPolicy live_audio_policy;
    std::map<QString,float> live_audio_gains;
    std::map<QString,std::map<QString,MAVideoRule>> committed_rules;
    MAVideoRule get_committed(const QString &scene,const QString &source)const{
        auto a=committed_rules.find(scene);if(a==committed_rules.end())return {};
        auto b=a->second.find(source);return b==a->second.end()?MAVideoRule{}:b->second;
    }
    void show_source(const QString &) {}
    void release_source(const QString &) {}
    bool shared_in_scene(const QString &scene,const QString &source)const{
        for(const auto &item:ma_video_sources_in_scene(scene))if(item.uuid==source)return true;
        return false;
    }
    // Decoder transport is doubled; enter/exit/gain and source traversal below
    // are the actual production methods. These scenarios use begin-start rules.
    void add_pending(const MAVideoCue &cue,const MAVideoEntry &item,const MAVideoRule &rule,const QString &){
        MAVideoPending p;p.source=item.uuid;p.scene=cue.target_uuid;p.rule=rule;p.fired=rule.start!="manual";pending.push_back(p);
    }
#include "blackmagic_rules_under_test.inc"
};
void near(float actual,float expected){assert(std::abs(actual-expected)<.00001f);}
int main(){
    obs_source_t cameraA{"camA","DeckLink A","decklink-input"},cameraB{"camB","DeckLink B","decklink-input"};
    obs_source_t cameraC{"camC","DeckLink C","decklink-input"},noAudio{"silent","DeckLink no audio","decklink-input",0};
    obs_source_t video{"video","Video","ffmpeg_source"},vlc{"vlc","VLC","vlc_source"};
    obs_source_t a{"a","A","scene"},b{"b","B","scene"},c{"c","C","scene"},group{"g","Group","group"};
    obs_scene_t sub{&group,{{&cameraA},{&video},{&noAudio}}};group.scene=&sub;
    obs_scene_t sa{&a,{{&group},{&cameraA},{&vlc}}};a.scene=&sa;
    obs_scene_t sb{&b,{{&cameraB}}};b.scene=&sb;
    obs_scene_t sc{&c,{{&cameraC}}};c.scene=&sc;
    registry={{"a",&a},{"b",&b},{"c",&c}};
    assert(!ma_blackmagic_audio_source(nullptr) && ma_blackmagic_audio_source(&cameraA));
    assert(!ma_blackmagic_audio_source(&noAudio) && !ma_blackmagic_audio_source(&video));
    assert(!ma_media_source(&cameraA));
    auto all=ma_control_sources_in_scene("a");assert(all.size()==3 && all[0].audio_only);
    assert(ma_video_sources_in_scene("a").size()==2); // only FFmpeg/VLC receive transport calls
    sub.items.push_back({&a});assert(ma_control_sources_in_scene("a").size()==3); // cycle + duplicate
    Engine engine;MAVideoCue cue;
    engine.committed_rules["a"]["camA"]={true,"fade"};
    engine.committed_rules["b"]["camB"]={true,"fade"};
    engine.committed_rules["c"]["camC"]={true,"fade"};
    for(float p:{0.f,.25f,.5f,.75f,1.f}){
        cue.progress=p;cue.reached=p==1;cue.live_layers={{"a","",1-p},{"b","",p}};
        engine.update_live_audio(cue);near(engine.live_audio_gains["camA"],1-p);near(engine.live_audio_gains["camB"],p);
        assert(engine.audio_gains()==engine.live_audio_gains);
    }
    // Retarget keeps the exact visible fractions of earlier layers.
    cue.target_uuid="c";cue.progress=.4f;cue.reached=false;cue.live_layers={{"a","",.42f},{"b","",.18f},{"c","",.4f}};
    engine.update_live_audio(cue);near(engine.live_audio_gains["camA"],.42f);near(engine.live_audio_gains["camB"],.18f);near(engine.live_audio_gains["camC"],.4f);
    engine.last_cue.progress=.4f;engine.pending.push_back({"video",{true,"fade"},true});
    auto combined=engine.audio_gains();near(combined["video"],.42f);near(combined["camA"],.42f);
    engine.pending.clear();
    engine.committed_rules["a"]["camA"].audio_mode="full";
    engine.committed_rules["b"]["camB"].audio_mode="mute";
    engine.update_live_audio(cue);near(engine.live_audio_gains["camA"],1);near(engine.live_audio_gains["camB"],0);
    // End mode stays silent until reached; aborting it cannot turn it on.
    engine.live_audio_policy.clear();engine.committed_rules["c"]["camC"].audio_mode="end";
    engine.update_live_audio(cue);near(engine.live_audio_gains["camC"],0);
    cue.target_uuid="b";cue.live_layers={{"c","",.4f},{"b","",.6f}};
    engine.update_live_audio(cue);near(engine.live_audio_gains["camC"],0);
    cue.target_uuid="c";cue.progress=1;cue.reached=true;cue.live_layers={{"c","",1}};
    engine.update_live_audio(cue);near(engine.live_audio_gains["camC"],1);
    cue.target_uuid="b";cue.progress=.5f;cue.reached=false;cue.live_layers={{"c","",.5f},{"b","",.5f}};
    engine.update_live_audio(cue);near(engine.live_audio_gains["camC"],1);
    // One camera shared across scenes stays continuous and is never doubled.
    sb.items={{&cameraA}};cue.live_layers={{"a","",.5f},{"b","",.5f}};
    engine.committed_rules["a"]["camA"]={true,"fade"};engine.committed_rules["b"]["camA"]={true,"fade"};
    engine.update_live_audio(cue);assert(engine.live_audio_gains.size()==1);near(engine.live_audio_gains["camA"],1);
    engine.committed_rules["b"]["camA"].audio_mode="mute";
    engine.update_live_audio(cue);near(engine.live_audio_gains["camA"],.5f);
    // Muted and hidden uses must not inflate a shared source's audio gain.
    sa.items[0].visible=false;sa.items[1].visible=false;
    engine.update_live_audio(cue);near(engine.live_audio_gains["camA"],0);
    sa.items[0].visible=true;group.muted=true;
    engine.update_live_audio(cue);near(engine.live_audio_gains["camA"],0);
    group.muted=false;sa.items[1].visible=true;
    engine.committed_rules.clear();engine.update_live_audio(cue);assert(engine.live_audio_gains.empty());
    engine.committed_rules["a"]["camA"]={true,"fade"};
    control_active=false;engine.update_live_audio(cue);assert(engine.live_audio_gains.empty());
    control_active=true;engine.enabled=false;engine.update_live_audio(cue);assert(engine.live_audio_gains.empty());
    // Regression: A -> B at 30%, then -> C must keep A=.7 and B=.3.
    obs_source_t videoB{"videoB","Video B","ffmpeg_source"},videoC{"videoC","Video C","ffmpeg_source"};
    sa.items={{&video}};sb.items={{&videoB}};sc.items={{&videoC}};
    registry["video"]=&video;registry["videoB"]=&videoB;registry["videoC"]=&videoC;
    Engine fades;fades.got_event=false;
    for(const auto &pair:std::vector<std::pair<const char *,const char *>>{{"a","video"},{"b","videoB"},{"c","videoC"}})
        fades.committed_rules[pair.first][pair.second]={true,"fade"};
    auto advance=[&](const char *target,uint64_t event,float progress,std::vector<MALiveLayer> layers){
        MAVideoCue frame;frame.target_uuid=target;frame.event=event;frame.progress=progress;
        frame.reached=progress==1;frame.live_layers=std::move(layers);fades.last_cue=frame;
        if(!fades.got_event || fades.cue_event!=event)fades.enter(frame,event*1000);
        if(frame.reached)fades.finish_outgoing(frame.target_uuid);
        fades.last_progress=progress;fades.update_live_audio(frame);return fades.audio_gains();
    };
    auto gains=advance("a",1,1,{{"a","",1}});near(gains["video"],1);
    gains=advance("b",2,.3f,{{"a","",.7f},{"b","",.3f}});near(gains["video"],.7f);near(gains["videoB"],.3f);
    gains=advance("c",3,0,{{"a","",.7f},{"b","",.3f}});near(gains["video"],.7f);near(gains["videoB"],.3f);near(gains["videoC"],0);
    assert(fades.continued_audio.count("video"));
    gains=advance("c",3,.5f,{{"a","",.35f},{"b","",.15f},{"c","",.5f}});
    near(gains["video"],.35f);near(gains["videoB"],.15f);near(gains["videoC"],.5f);
    gains=advance("a",4,0,{{"a","",.35f},{"b","",.15f},{"c","",.5f}});
    near(gains["video"],.35f);near(gains["videoB"],.15f);near(gains["videoC"],.5f);
    std::vector<MALiveLayer> base{{"a","",.35f},{"b","",.15f},{"c","",.5f}};
    for(int event=5;event<105;++event){
        const char *target=event%2?"b":"c";
        const auto before=fades.audio_gains();const auto at_zero=advance(target,event,0,base);
        for(const char *uuid:{"video","videoB","videoC"})near(at_zero.at(uuid),before.at(uuid));
        for(auto &layer:base)layer.weight*=.8f;
        for(auto &layer:base)if(layer.uuid==target)layer.weight+=.2f;
        gains=advance(target,event,.2f,base);
        for(const auto &item:gains)assert(item.second>=0 && item.second<=1);
    }
    gains=advance("c",105,1,{{"c","",1}});near(gains["videoC"],1);near(gains["video"],0);
    // Shared decoder contributions add once and are capped; an unstarted
    // delayed/manual source stays silent even while its scene is visible.
    sb.items={{&video}};fades.pending.clear();fades.outgoing.clear();fades.continued_sources.clear();fades.continued_audio.clear();
    fades.pending.push_back({"video",{true,"fade"},true});
    cue.live_layers={{"a","",.6f},{"b","",.4f}};fades.update_live_audio(cue);near(fades.audio_gains()["video"],1);
    fades.pending[0].fired=false;near(fades.audio_gains()["video"],0);
    // A cached Goto restart choice cannot override a manual-only rule.
    fades.pending[0].rule.start="manual";fades.pending[0].rule.same_goto="restart";
    fades.got_event=true;fades.cue_scene="a";cue.target_uuid="a";
    const auto commands_before=media_commands.size();fades.enter(cue,99999);
    assert(media_commands.size()==commands_before && !fades.pending[0].fired);
    for(auto *s:{&cameraA,&cameraB,&cameraC,&video,&vlc,&a,&b,&c,&group,&noAudio})assert(s->refs==0);
}
