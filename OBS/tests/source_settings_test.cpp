#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct obs_data_t {
    int refs=1;
    std::map<std::string,bool> values,defaults;
    std::map<std::string,long long> ints;
    std::map<std::string,obs_data_t *> objects;
};
static int data_count=0;
obs_data_t *obs_data_create() {++data_count; return new obs_data_t;}
void obs_data_release(obs_data_t *d) {
    if (!d || --d->refs) return;
    for (auto &o:d->objects) obs_data_release(o.second);
    delete d; --data_count;
}
void obs_data_set_bool(obs_data_t *d,const char *k,bool v) {d->values[k]=v;}
bool obs_data_get_bool(obs_data_t *d,const char *k) {
    if (!d) return false;
    auto i=d->values.find(k); return i!=d->values.end()?i->second:d->defaults[k];
}
bool obs_data_has_user_value(obs_data_t *d,const char *k) {return d->values.count(k)>0;}
void obs_data_unset_user_value(obs_data_t *d,const char *k) {d->values.erase(k);}
void obs_data_set_int(obs_data_t *d,const char *k,long long v) {d->ints[k]=v;}
long long obs_data_get_int(obs_data_t *d,const char *k) {return d?d->ints[k]:0;}
void obs_data_erase(obs_data_t *d,const char *k) {
    auto i=d->objects.find(k);
    if (i!=d->objects.end()) {obs_data_release(i->second);d->objects.erase(i);}
}
void obs_data_set_obj(obs_data_t *d,const char *k,obs_data_t *o) {
    ++o->refs; obs_data_erase(d,k); d->objects[k]=o;
}
obs_data_t *obs_data_get_obj(obs_data_t *d,const char *k) {
    if (!d || !d->objects.count(k)) return nullptr;
    auto *o=d->objects[k]; ++o->refs; return o;
}
struct obs_source_t {
    std::string name="media",id="ffmpeg_source";
    obs_data_t *settings=obs_data_create(),*private_data=obs_data_create();
    int refs=1,updates=0,ma_filters=0,user_filters=2;
    ~obs_source_t() {assert(refs==1);obs_data_release(settings);obs_data_release(private_data);}
};
bool ma_ffmpeg_source(obs_source_t *s) {return s && s->id=="ffmpeg_source";}
bool ma_media_source(obs_source_t *s) {return s && (ma_ffmpeg_source(s)||s->id=="vlc_source");}
bool ma_blackmagic_audio_source(obs_source_t *s) {return s && s->id=="decklink-input";}
obs_data_t *obs_source_get_private_settings(obs_source_t *s) {++s->private_data->refs;return s->private_data;}
obs_data_t *obs_source_get_settings(obs_source_t *s) {++s->settings->refs;return s->settings;}
static bool enumerating=false;
void obs_source_update(obs_source_t *s,obs_data_t *d) {assert(!enumerating && d==s->settings);++s->updates;}
const char *obs_source_get_name(obs_source_t *s) {return s->name.c_str();}
obs_source_t *obs_source_get_ref(obs_source_t *s) {++s->refs;return s;}
void obs_source_release(obs_source_t *s) {--s->refs;assert(s->refs>=1);}
static std::vector<obs_source_t *> sources;
void obs_enum_sources(bool (*cb)(void *,obs_source_t *),void *data) {
    enumerating=true;for(auto *s:sources) if(!cb(data,s)) break;enumerating=false;
}
void ma_audio_remove_stale_filters(obs_source_t *s) {assert(!enumerating);s->ma_filters=0;}
constexpr int LOG_INFO=0,LOG_WARNING=1;
void blog(int,const char *,...) {}
#include "../src/source_settings.h"
static bool control_active=true;
static bool ma_media_control_active(){return control_active;}
struct MAVideoRule {std::string on_end="loop",before="first";};
#include "prepare_under_test.inc"
int main() {
    for (unsigned values=0;values<16;++values) for (unsigned users=0;users<16;++users) {
        obs_source_t source;
        unsigned index=0;
        for (const char *key:ma_borrowed_media_keys) {
            bool value=(values&(1u<<index))!=0;
            source.settings->defaults[key]=value;
            if (users&(1u<<index)) obs_data_set_bool(source.settings,key,value);
            ++index;
        }
        assert(ma_remember_media_settings(&source,source.settings));
        for (const char *key:ma_borrowed_media_keys)
            obs_data_set_bool(source.settings,key,!obs_data_get_bool(source.settings,key));
        // Repeated cue preparation cannot replace the first snapshot.
        assert(ma_remember_media_settings(&source,source.settings));
        obs_data_set_bool(source.settings,"user_unrelated_edit",true);
        obs_data_set_bool(source.private_data,"user_private_data",true);
        assert(ma_restore_media_settings(&source));
        index=0;
        for (const char *key:ma_borrowed_media_keys) {
            assert(obs_data_get_bool(source.settings,key)==bool(values&(1u<<index)));
            assert(obs_data_has_user_value(source.settings,key)==bool(users&(1u<<index)));
            ++index;
        }
        assert(obs_data_get_bool(source.settings,"user_unrelated_edit"));
        assert(obs_data_get_bool(source.private_data,"user_private_data"));
        assert(source.user_filters==2 && source.updates==1);
        assert(!ma_restore_media_settings(&source) && source.updates==1);
        // A new activation takes a fresh baseline, including off-mode edits.
        obs_data_set_bool(source.settings,"looping",true);
        assert(ma_remember_media_settings(&source,source.settings));
        obs_data_set_bool(source.settings,"looping",false);
        assert(ma_restore_media_settings(&source));
        assert(obs_data_get_bool(source.settings,"looping"));
    }
    {
        obs_source_t source;
        obs_data_set_bool(source.settings,"restart_on_activate",true);
        obs_data_set_bool(source.settings,"close_when_inactive",true);
        MAVideoRule rule;
        control_active=false;
        assert(!ma_video_apply_ffmpeg(&source,rule));
        ma_video_restore_hold(&source);assert(source.updates==0 && source.private_data->objects.empty());
        control_active=true;
        assert(ma_video_apply_ffmpeg(&source,rule));
        assert(!obs_data_get_bool(source.settings,"restart_on_activate"));
        assert(!obs_data_get_bool(source.settings,"close_when_inactive"));
        assert(obs_data_get_bool(source.settings,"looping"));
        // Later MA rules and temporary preload flags keep the first baseline.
        rule.on_end="hold";assert(ma_video_apply_ffmpeg(&source,rule));
        ma_video_restore_hold(&source);
        assert(!obs_data_get_bool(source.settings,"clear_on_media_end"));
        assert(ma_restore_media_settings(&source));
        assert(obs_data_get_bool(source.settings,"restart_on_activate"));
        assert(obs_data_get_bool(source.settings,"close_when_inactive"));
        assert(!obs_data_has_user_value(source.settings,"looping"));
        int updates=source.updates;
        control_active=false;assert(!ma_video_apply_ffmpeg(&source,rule));assert(source.updates==updates);
        // User changes an option while OFF; reactivation adopts that baseline.
        obs_data_set_bool(source.settings,"looping",true);
        control_active=true;assert(ma_video_apply_ffmpeg(&source,rule));
        assert(!obs_data_get_bool(source.settings,"looping"));
        assert(ma_restore_media_settings(&source) && obs_data_get_bool(source.settings,"looping"));
    }
    {
        obs_source_t media,vlc,text,blackmagic;
        blackmagic.id="decklink-input";blackmagic.ma_filters=3;
        vlc.id="vlc_source";text.id="text_gdiplus_v3";
        media.ma_filters=3;vlc.ma_filters=2;text.ma_filters=1;
        obs_data_set_bool(media.settings,"looping",false);
        assert(ma_remember_media_settings(&media,media.settings));
        obs_data_set_bool(media.settings,"looping",true);
        // Simulated persisted/reloaded private data: recovery has no runtime map.
        obs_source_t loaded;
        auto *persisted=obs_data_get_obj(media.private_data,ma_original_media_key);
        obs_data_set_obj(loaded.private_data,ma_original_media_key,persisted);obs_data_release(persisted);
        obs_data_set_bool(loaded.settings,"looping",true);
        sources={&loaded,&media,&vlc,&text,&blackmagic};
        ma_restore_media_sources();
        assert(!obs_data_get_bool(loaded.settings,"looping"));
        assert(!obs_data_get_bool(media.settings,"looping"));
        assert(media.ma_filters==0 && vlc.ma_filters==0 && text.ma_filters==1);
        assert(media.user_filters==2 && vlc.user_filters==2);
        assert(text.updates==0 && vlc.updates==0);
        assert(blackmagic.updates==0 && blackmagic.ma_filters==0 && blackmagic.user_filters==2);
        sources.clear();
    }
    assert(data_count==0);
}
