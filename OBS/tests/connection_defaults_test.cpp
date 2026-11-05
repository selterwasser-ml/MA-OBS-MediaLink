#include "../src/connection_defaults.h"
#include <cassert>
#include <map>
#include <string>

struct obs_data_t {
    std::map<std::string,std::string> values, defaults;
};
static bool file_exists = false;
static obs_data_t saved;
static std::string native_config_path() { return "native_settings.json"; }
static obs_data_t *obs_data_create() { return new obs_data_t; }
static obs_data_t *obs_data_create_from_json_file_safe(const char *, const char *) {
    return file_exists ? new obs_data_t(saved) : nullptr;
}
static void obs_data_set_string(obs_data_t *d,const char *key,const char *value) {d->values[key]=value;}
static void obs_data_set_int(obs_data_t *d,const char *key,long long value) {d->values[key]=std::to_string(value);}
static void obs_data_set_default_string(obs_data_t *d,const char *key,const char *value) {d->defaults[key]=value;}
static void obs_data_set_default_int(obs_data_t *d,const char *key,long long value) {d->defaults[key]=std::to_string(value);}
static void obs_data_set_default_bool(obs_data_t *d,const char *key,bool value) {d->defaults[key]=value?"true":"false";}
static std::string get(obs_data_t *d,const char *key) {
    const auto found=d->values.find(key);
    return found==d->values.end()?d->defaults.at(key):found->second;
}
#include "connection_defaults_under_test.inc"

int main() {
    auto *fresh=native_load();
    assert(get(fresh,"universe")=="1");
    assert(get(fresh,"multicast_interface")=="0.0.0.0");
    assert(get(fresh,"bind_ip")=="0.0.0.0");
    delete fresh;
    saved.values={{"universe","12"},{"multicast_interface","192.168.1.71"},
                  {"network_mode","multicast"},{"width","3840"},{"height","2160"}};
    file_exists=true;
    auto *upgraded=native_load();
    assert(get(upgraded,"universe")=="12");
    assert(get(upgraded,"multicast_interface")=="192.168.1.71");
    assert(get(upgraded,"network_mode")=="multicast");
    assert(get(upgraded,"width")=="3840" && get(upgraded,"height")=="2160");
    assert(get(upgraded,"address")=="1");
    assert(upgraded->values==saved.values);
    delete upgraded;
    saved.values={{"universe","88"}};
    auto *legacy=native_load();
    assert(get(legacy,"universe")=="88");
    assert(get(legacy,"multicast_interface")=="0.0.0.0");
    assert(legacy->values==saved.values);
    delete legacy;
    obs_data_t source;
    defaults(&source);
    assert(get(&source,"universe")=="1");
    assert(get(&source,"multicast_interface")=="0.0.0.0");
    source.values={{"universe","5"},{"multicast_interface","10.20.30.40"}};
    defaults(&source);
    assert(get(&source,"universe")=="5");
    assert(get(&source,"multicast_interface")=="10.20.30.40");
}
