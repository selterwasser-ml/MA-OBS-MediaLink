// Native FFmpeg options are borrowed only while MediaLink controls the source.
// The backup lives in OBS private source data, so saving an active collection
// never loses its original values. It holds no OBS source references.
#pragma once
static constexpr const char *ma_original_media_key="ma_obs_medialink_original_v1";
static constexpr const char *ma_borrowed_media_keys[]={
    "restart_on_activate", "close_when_inactive", "looping", "clear_on_media_end"
};

static bool ma_remember_media_settings(obs_source_t *source, obs_data_t *settings) {
    obs_data_t *private_data=obs_source_get_private_settings(source);
    if (!private_data) return false;
    obs_data_t *original=obs_data_get_obj(private_data,ma_original_media_key);
    if (original) {
        const bool supported=obs_data_get_int(original,"schema")==1;
        obs_data_release(original); obs_data_release(private_data);
        return supported; // never replace the first baseline with an MA value
    }
    original=obs_data_create();
    obs_data_set_int(original,"schema",1);
    for (const char *key:ma_borrowed_media_keys) {
        obs_data_set_bool(original,key,obs_data_get_bool(settings,key));
        const std::string flag=std::string("user_")+key;
        obs_data_set_bool(original,flag.c_str(),obs_data_has_user_value(settings,key));
    }
    obs_data_set_obj(private_data,ma_original_media_key,original);
    obs_data_release(original); obs_data_release(private_data);
    return true;
}

static bool ma_restore_media_settings(obs_source_t *source) {
    if (!ma_ffmpeg_source(source)) return false;
    obs_data_t *private_data=obs_source_get_private_settings(source);
    obs_data_t *original=obs_data_get_obj(private_data,ma_original_media_key);
    if (!original || obs_data_get_int(original,"schema")!=1) {
        obs_data_release(original); obs_data_release(private_data); return false;
    }
    obs_data_t *settings=obs_source_get_settings(source);
    if (!settings) {
        obs_data_release(original); obs_data_release(private_data); return false;
    }
    bool changed=false;
    for (const char *key:ma_borrowed_media_keys) {
        const std::string flag=std::string("user_")+key;
        const bool had_user=obs_data_get_bool(original,flag.c_str());
        const bool value=obs_data_get_bool(original,key);
        changed=changed || obs_data_has_user_value(settings,key)!=had_user ||
            obs_data_get_bool(settings,key)!=value;
        if (had_user) obs_data_set_bool(settings,key,value);
        else obs_data_unset_user_value(settings,key);
    }
    // get_settings() returns the source's actual data. Unsetting there also
    // restores implicit defaults, rather than leaving an explicit override.
    if (changed) obs_source_update(source,settings);
    obs_data_erase(private_data,ma_original_media_key);
    obs_data_release(settings); obs_data_release(original); obs_data_release(private_data);
    return true;
}

static void ma_restore_media_sources() {
    std::vector<obs_source_t *> sources;
    obs_enum_sources([](void *data,obs_source_t *source) {
        if (ma_media_source(source) || ma_blackmagic_audio_source(source)) {
            obs_source_t *ref=obs_source_get_ref(source);
            if (ref) static_cast<std::vector<obs_source_t *> *>(data)->push_back(ref);
        }
        return true;
    },&sources);
    // Do not mutate filter lists or run source updates under the enumeration lock.
    for (auto *source:sources) {
        ma_audio_remove_stale_filters(source);
        if (ma_restore_media_settings(source))
            blog(LOG_INFO,"MA OBS: original media settings restored: %s",obs_source_get_name(source));
        obs_source_release(source);
    }
}
