#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>
constexpr size_t MAX_AUDIO_MIXES=2, MAX_AUDIO_CHANNELS=2, AUDIO_OUTPUT_FRAMES=4;
constexpr int OBS_SOURCE_AUDIO=1, OBS_SOURCE_COMPOSITE=2;
struct obs_source_t;
struct obs_sceneitem_t {obs_source_t *source; bool visible=true;};
struct obs_scene_t {std::vector<obs_sceneitem_t> items;};
struct obs_source_audio_mix {struct {float *data[2]{};} output[2];};
struct obs_source_t {
 std::string uuid; obs_scene_t *scene=nullptr; bool mute=false, pending=false;
 float volume=1; uint32_t tracks=3; int refs=0;
 float samples[4]{1,1,1,1}; uint64_t ts=1000000000;
};
static std::map<std::string,obs_source_t*> sources;
obs_source_t *obs_source_get_ref(obs_source_t *s){++s->refs;return s;}
void obs_source_release(obs_source_t *s){--s->refs;assert(s->refs>=0);}
obs_scene_t *obs_scene_from_source(obs_source_t *s){return s->scene;}
obs_scene_t *obs_group_from_source(obs_source_t *){return nullptr;}
bool obs_source_muted(obs_source_t *s){return s->mute;}
float obs_source_get_volume(obs_source_t *s){return s->volume;}
uint32_t obs_source_get_audio_mixers(obs_source_t *s){return s->tracks;}
bool obs_sceneitem_visible(obs_sceneitem_t *i){return i->visible;}
obs_source_t *obs_sceneitem_get_source(obs_sceneitem_t *i){return i->source;}
void obs_scene_enum_items(obs_scene_t *s,bool(*f)(obs_scene_t*,obs_sceneitem_t*,void*),void *p){for(auto &i:s->items)if(!f(s,&i,p))break;}
int obs_source_get_output_flags(obs_source_t *){return OBS_SOURCE_AUDIO;}
const char *obs_source_get_uuid(obs_source_t *s){return s->uuid.c_str();}
obs_source_t *obs_get_source_by_uuid(const char *id){auto it=sources.find(id);return it==sources.end()?nullptr:obs_source_get_ref(it->second);}
obs_source_t *obs_get_source_by_name(const char *id){return obs_get_source_by_uuid(id);}
bool obs_source_audio_pending(obs_source_t *s){return s->pending;}
uint64_t obs_source_get_audio_timestamp(obs_source_t *s){return s->ts;}
void obs_source_get_audio_mix(obs_source_t *s,obs_source_audio_mix *out){for(auto &m:out->output)for(auto &ch:m.data)ch=s->samples;}
struct MALiveLayer{std::string uuid,name;float weight;};
struct Source{obs_source_t *owner=nullptr;std::vector<MALiveLayer> layers;};
std::vector<MALiveLayer> ma_live_frame_layers(Source *s){return s->layers;}
#include "../src/audio_sources.h"
#include "audio_render_under_test.inc"
float render(Source &s, size_t track=0){float buffer[2][2][4]{};obs_source_audio_mix out{};
 for(int m=0;m<2;++m)for(int c=0;c<2;++c)out.output[m].data[c]=buffer[m][c];
 uint64_t ts=0;ma_audio_render(&s,&ts,&out,3,2,48000);return buffer[track][0][0];}
int main(){
 obs_source_t a{"a"},b{"b"},hidden{"hidden"},root{"root"},other{"other"};
 obs_scene_t scene{{{&a},{&b},{&hidden,false},{&a}}};root.scene=&scene;
 obs_scene_t second{{{&a},{&b}}};other.scene=&second;
 sources={{"root",&root},{"other",&other}};
 Source s; s.layers={{"root","",.25f}};
 ma_audio_managed={"a","b"};
 // A's runtime filter supplies .25, B keeps 1: no second .25 multiplication.
 std::fill_n(a.samples,4,.25f);assert(std::abs(render(s)-1.25f)<1e-6);
 s.layers={{"root","",.75f},{"other","",.25f}};
 assert(std::abs(render(s)-1.25f)<1e-6); // shared sources once
 ma_audio_managed.clear();assert(std::abs(render(s)-1.25f)<1e-6);
 s.layers={{"root","",.25f}};assert(std::abs(render(s)-.3125f)<1e-6);
 ma_audio_managed={"a","b"};root.volume=.5f;assert(std::abs(render(s)-.625f)<1e-6);
 root.tracks=1;assert(render(s,1)==0);root.mute=true;assert(render(s)==0);
 root.mute=false;root.volume=1;root.tracks=3;
 b.pending=true;assert(std::abs(render(s)-.25f)<1e-6);b.pending=false;
 b.ts+=1000000000;assert(std::abs(render(s)-.25f)<1e-6);b.ts-=1000000000;
 // Circular scene references must terminate; duplicate leaves remain single.
 scene.items.push_back({&root});assert(std::abs(render(s)-1.25f)<1e-6);
 s.layers.clear();assert(render(s)==0);
 for(auto *p:{&a,&b,&hidden,&root,&other})assert(p->refs==0);
}
