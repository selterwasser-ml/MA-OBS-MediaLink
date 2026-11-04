#include <cassert>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
struct QByteArray {std::string value;const char *constData() const {return value.c_str();}};
struct QString {
    std::string value;
    QString()=default;QString(const char *s):value(s){}
    QByteArray toUtf8() const {return {value};}
    bool operator==(const QString &s) const {return value==s.value;}
    bool operator<(const QString &s) const {return value<s.value;}
};
struct MAVideoRule {bool enabled=false;int marker=0;};
struct MAVideoEntry {QString uuid,name;bool ffmpeg=true;};
struct MAVideoCue {QString target_uuid="scene";};
struct MAVideoPending {
    QString source;
    MAVideoRule rule;
    bool fired=false,manual_override=false;
    int position=0;
};
static std::map<QString,std::vector<MAVideoEntry>> catalog;
std::vector<MAVideoEntry> ma_video_sources_in_scene(const QString &scene) {return catalog[scene];}
struct obs_data_t {};
struct obs_data_array_t {};
static bool save_success=true;
std::string ma_video_rules_path() {return "test-rules.json";}
obs_data_t *obs_data_create() {return new obs_data_t;}
obs_data_t *ma_video_rule_save(const MAVideoRule &) {return obs_data_create();}
obs_data_array_t *obs_data_array_create() {return new obs_data_array_t;}
void obs_data_release(obs_data_t *d) {delete d;}
void obs_data_array_release(obs_data_array_t *d) {delete d;}
void obs_data_set_string(obs_data_t *,const char *,const char *) {}
void obs_data_set_bool(obs_data_t *,const char *,bool) {}
void obs_data_array_push_back(obs_data_array_t *,obs_data_t *) {}
void obs_data_set_array(obs_data_t *,const char *,obs_data_array_t *) {}
bool obs_data_save_json_safe(obs_data_t *,const char *,const char *,const char *) {return save_success;}
constexpr int LOG_WARNING=1;
void blog(int,const char *) {}
struct Engine {
    bool enabled=true;
    QString collection="test";
    uint64_t last_membership_ms=99;
    std::map<QString,std::map<QString,MAVideoRule>> rules,committed_rules;
    std::vector<MAVideoPending> pending,outgoing;
    std::set<QString> continued_sources;
    std::vector<QString> additions,releases;
    void release_source(const QString &source) {releases.push_back(source);}
    void add_pending(const MAVideoCue &,const MAVideoEntry &entry,const MAVideoRule &rule,const QString &) {
        additions.push_back(entry.uuid);pending.push_back({entry.uuid,rule,false,false,0});
    }
#include "membership_under_test.inc"
};
int main() {
    Engine engine;MAVideoCue cue;
    catalog["scene"]={{"a","A",true},{"b","B",true}};
    engine.committed_rules["scene"]["a"]={true,1};
    engine.rules=engine.committed_rules;
    engine.pending.push_back({"a",{true,1},true,true,42000});
    engine.sync_scene_membership(cue);
    assert(engine.pending.size()==1 && engine.additions.empty());
    assert(engine.pending[0].position==42000 && engine.pending[0].manual_override);
    // Draft activation is invisible to the live list, including failed saves.
    engine.rules["scene"]["b"]={true,2};save_success=false;
    assert(!engine.save());assert(engine.last_membership_ms==99);
    engine.sync_scene_membership(cue);assert(engine.pending.size()==1);
    save_success=true;assert(engine.save());assert(engine.last_membership_ms==0);
    engine.sync_scene_membership(cue);
    assert(engine.pending.size()==2 && engine.additions.size()==1 && engine.additions[0]==QString("b"));
    engine.pending[1].position=9000;
    // Reordering/renaming and duplicate references must preserve live entries.
    catalog["scene"]={{"b","renamed",true},{"a","A",true},{"b","renamed",true}};
    engine.sync_scene_membership(cue);
    assert(engine.additions.size()==1 && engine.pending[0].position==42000 && engine.pending[1].position==9000);
    // Updating a rule must not recreate an already-running source.
    engine.rules["scene"]["a"].marker=77;assert(engine.save());
    engine.sync_scene_membership(cue);assert(engine.pending[0].rule.marker==1);
    // Removing A releases only its ownership; B is unaffected.
    catalog["scene"]={{"b","B",true}};engine.sync_scene_membership(cue);
    assert(engine.pending.size()==1 && engine.pending[0].source==QString("b"));
    assert(engine.pending[0].position==9000 && engine.releases.size()==1 && engine.releases[0]==QString("a"));
    // A saved enabled rule is reused when the same UUID is reinserted.
    catalog["scene"].push_back({"a","A",true});engine.sync_scene_membership(cue);
    assert(engine.pending.size()==2 && engine.pending[1].rule.marker==77);
    // A decoder retained by another fade/continue rule is not released.
    engine.outgoing.push_back({"a",{true,1},true,false,42});
    catalog["scene"]={{"b","B",true}};engine.sync_scene_membership(cue);
    assert(engine.releases.size()==1);
    engine.continued_sources.insert("b");catalog["scene"].clear();engine.sync_scene_membership(cue);
    assert(engine.pending.empty() && engine.releases.size()==1);
    engine.continued_sources.clear();engine.outgoing.clear();
    catalog["scene"]={{"b","B",true}};engine.sync_scene_membership(cue);
    engine.rules["scene"]["b"].enabled=false;assert(engine.save());engine.sync_scene_membership(cue);
    assert(engine.pending.empty() && engine.releases.size()==2);
}
