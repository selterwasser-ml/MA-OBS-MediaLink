#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
struct obs_data_t {
    std::map<std::string,bool> flags;
    std::map<std::string,std::string> other;
};
static obs_data_t disk;
static bool exists=false,path_available=true,save_ok=true;
static int refs=0,paths=0,writes=0,warnings=0;
char *obs_module_config_path(const char *name){
    assert(std::string(name)=="monitor_ui.json");
    if(!path_available)return nullptr;
    ++paths;return strdup(name);
}
void bfree(char *p){--paths;free(p);}
obs_data_t *obs_data_create(){++refs;return new obs_data_t;}
obs_data_t *obs_data_create_from_json_file_safe(const char *,const char *){
    if(!exists)return nullptr;
    ++refs;return new obs_data_t(disk);
}
void obs_data_release(obs_data_t *p){--refs;delete p;}
bool obs_data_has_user_value(obs_data_t *p,const char *key){return p->flags.count(key);}
bool obs_data_get_bool(obs_data_t *p,const char *key){return p->flags[key];}
void obs_data_set_bool(obs_data_t *p,const char *key,bool value){p->flags[key]=value;}
bool obs_data_save_json_safe(obs_data_t *p,const char *,const char *,const char *){
    if(!save_ok)return false;
    disk=*p;exists=true;++writes;return true;
}
constexpr int LOG_WARNING=1;
void blog(int,const char *){++warnings;}
#include "dock_preferences_under_test.inc"
struct QWidget {
    bool visible=true,diagnostic_property=false;
    int scroll_v=73,scroll_h=11;
    void setVisible(bool v){visible=v;}
    void setProperty(const char *name,bool value){assert(std::string(name)=="ma_diagnostics");diagnostic_property=value;}
};
struct Dock {
    QWidget diagnosis,video,scenes;
    QWidget *diagnostics=&diagnosis,*video_panel=&video,*scene_list=&scenes;
#include "dock_visibility_under_test.inc"
};
int main(){
    assert(!ma_read_diagnostics_preference() && ma_read_scene_list_preference());
    assert(!exists && writes==0); // first open is a read, not a migration write
    exists=true;disk.flags["diagnostics"]=true;disk.other["future_setting"]="keep";
    assert(ma_read_diagnostics_preference() && ma_read_scene_list_preference());
    for(bool diagnostic:{false,true})for(bool scenes:{false,true}){
        Dock live;
        live.set_diagnostics_visible(diagnostic);live.set_scene_list_visible(scenes);
        assert(live.diagnosis.visible==diagnostic && live.scenes.visible==scenes);
        assert(live.video.diagnostic_property==diagnostic && disk.other["future_setting"]=="keep");
        assert(live.scenes.scroll_v==73 && live.scenes.scroll_h==11);
        Dock reopened;const int saved=writes;
        reopened.set_diagnostics_visible(ma_read_diagnostics_preference(),false);
        reopened.set_scene_list_visible(ma_read_scene_list_preference(),false);
        assert(writes==saved && reopened.diagnosis.visible==diagnostic && reopened.scenes.visible==scenes);
        reopened.set_diagnostics_visible(!diagnostic);
        assert(ma_read_scene_list_preference()==scenes && reopened.scenes.visible==scenes);
        reopened.set_scene_list_visible(!scenes);
        assert(ma_read_diagnostics_preference()==!diagnostic && reopened.diagnosis.visible==!diagnostic);
    }
    // The diagnostics-only page has no media panel and does not modify dock
    // preferences when its initial full view is created.
    Dock panel;panel.video_panel=nullptr;const int saved=writes;
    panel.set_diagnostics_visible(true,false);panel.set_scene_list_visible(true,false);
    assert(writes==saved);
    save_ok=false;const auto before=disk.flags;
    ma_write_scene_list_preference(true);assert(disk.flags==before && warnings==1);
    path_available=false;assert(!ma_read_diagnostics_preference() && ma_read_scene_list_preference());
    ma_write_scene_list_preference(false);assert(disk.flags==before);
    assert(refs==0 && paths==0);
}
