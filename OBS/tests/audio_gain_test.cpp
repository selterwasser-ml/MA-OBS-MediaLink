#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>
constexpr size_t MAX_AV_PLANES=2;
struct QByteArray {std::string s; const char *constData() const {return s.c_str();}};
struct QString {
    std::string s; QString(const char *v):s(v){} QString()=default;
    QByteArray toUtf8() const {return {s};}
    bool operator<(const QString &o) const {return s<o.s;}
};
struct obs_data_t {double gain=0;};
struct obs_source_t {
    std::string id,uuid;
    int refs=1;
    void *data=nullptr;
    std::vector<obs_source_t*> filters;
};
struct obs_audio_data {uint8_t *data[MAX_AV_PLANES]{};uint32_t frames=0;};
static std::map<std::string,obs_source_t*> sources;
static std::mutex ma_audio_policy_mutex;
static std::set<std::string> ma_audio_managed;
obs_data_t *obs_data_create() {return new obs_data_t;}
void obs_data_release(obs_data_t *d) {delete d;}
void obs_data_set_double(obs_data_t *d,const char *,double value) {d->gain=value;}
double obs_data_get_double(obs_data_t *d,const char *) {return d->gain;}
obs_source_t *obs_source_get_ref(obs_source_t *s) {++s->refs;return s;}
void obs_source_release(obs_source_t *);
const char *obs_source_get_unversioned_id(obs_source_t *s) {return s->id.c_str();}
obs_source_t *obs_get_source_by_uuid(const char *id) {auto it=sources.find(id);return it==sources.end()?nullptr:obs_source_get_ref(it->second);}
void obs_source_enum_filters(obs_source_t *s,void(*cb)(obs_source_t*,obs_source_t*,void*),void *data) {
    for(auto *f:s->filters) cb(s,f,data);
}
void obs_source_filter_add(obs_source_t *s,obs_source_t *f) {s->filters.push_back(obs_source_get_ref(f));}
void obs_source_filter_remove(obs_source_t *s,obs_source_t *f) {
    auto it=std::find(s->filters.begin(),s->filters.end(),f);
    if(it!=s->filters.end()) {s->filters.erase(it);obs_source_release(f);}
}
obs_source_t *obs_source_create_private(const char *,const char *,obs_data_t *);
void obs_source_update(obs_source_t *,obs_data_t *);
#include "../src/audio_control.h"
void obs_source_release(obs_source_t *s) {
    if(--s->refs) return;
    assert(s->filters.empty());
    if(s->data) ma_audio_gain_destroy(s->data);
    delete s;
}
obs_source_t *obs_source_create_private(const char *id,const char *,obs_data_t *settings) {
    auto *s=new obs_source_t;s->id=id;s->data=ma_audio_gain_create(settings,s);return s;
}
void obs_source_update(obs_source_t *s,obs_data_t *d) {
    assert(s->id=="ma_obs_runtime_audio_gain"); // never update/reopen the input device
    ma_audio_gain_update(s->data,d);
}
static void block(obs_source_t *f,float expected) {
    float samples[4]={1,1,1,1};
    obs_audio_data audio;audio.frames=4;audio.data[0]=reinterpret_cast<uint8_t*>(samples);
    ma_audio_gain_filter(f->data,&audio); // ramp
    std::fill(std::begin(samples),std::end(samples),1.f);
    ma_audio_gain_filter(f->data,&audio);
    for(float sample:samples) assert(std::abs(sample-expected)<0.0001f);
}
int main() {
    assert(ma_audio_gain_name(nullptr));
    obs_data_t old;old.gain=0;
    auto *source=new obs_source_t;source->uuid="a";sources["a"]=source;
    auto *stale=obs_source_create_private("ma_obs_runtime_audio_gain","old",&old);
    block(stale,1); // saved zero must not restore silence, even without MA active
    obs_source_filter_add(source,stale);obs_source_release(stale);
    auto *user=new obs_source_t;user->id="user_gain";obs_source_filter_add(source,user);
    ma_audio_gains.apply({{QString("a"),0.f}});
    assert(source->filters.size()==2 && source->filters[0]==user);
    auto *owned=source->filters[1];block(owned,0);
    assert(ma_audio_managed.count("a"));
    ma_audio_gain_save(owned->data,&old);
    assert(old.gain==1);block(owned,0); // save cannot change live cue attenuation
    ma_audio_gains.apply({{QString("a"),0.5f}});
    assert(source->filters[1]==owned);block(owned,0.5f);
    ma_audio_gains.apply({{QString("a"),1.f}});block(owned,1);
    ma_audio_gains.clear();assert(source->filters.size()==1 && source->filters[0]==user);
    assert(source->refs==1 && user->refs==2 && ma_audio_managed.empty());
    // Duplicate saved filters are removed by type, irrespective of their names.
    for(int i=0;i<2;++i) {
        auto *f=obs_source_create_private("ma_obs_runtime_audio_gain","duplicate",&old);
        obs_source_filter_add(source,f);obs_source_release(f);
    }
    ma_audio_gains.apply({{QString("a"),1.f}});
    assert(source->filters.size()==2);block(source->filters[1],1);
    ma_audio_gains.apply({});assert(source->filters.size()==1 && ma_audio_managed.empty());
    obs_source_filter_remove(source,user);obs_source_release(user);
    sources.clear();obs_source_release(source);
    auto *camera=new obs_source_t;camera->id="decklink-input";camera->uuid="camera";sources["camera"]=camera;
    ma_audio_gains.apply({{QString("camera"),0.f}});
    float first_samples[4]={1,1,1,1};obs_audio_data first;first.frames=4;
    first.data[0]=reinterpret_cast<uint8_t *>(first_samples);
    ma_audio_gain_filter(camera->filters[0]->data,&first);
    for(float sample:first_samples)assert(sample==0.f); // no first-block audio burst
    for(float gain:{0.f,.25f,.5f,1.f}) {
        ma_audio_gains.apply({{QString("camera"),gain}});
        assert(camera->filters.size()==1 && ma_audio_managed.count("camera"));
        block(camera->filters[0],gain);
    }
    ma_audio_gains.clear();
    assert(camera->filters.empty() && camera->refs==1 && ma_audio_managed.empty());
    sources.clear();obs_source_release(camera);
}
