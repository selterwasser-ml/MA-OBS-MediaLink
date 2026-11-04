// MA OBS v0.25c: transient gain filters for managed media sources.
// This header is included inside plugin.cpp's anonymous namespace.
// OBS may serialize attached filters with their parent; saved gains are neutral.
#pragma once

struct MAControlGainFilter {
    std::atomic<float> gain{1.0f};
    float current_gain=1.0f; // audio thread only; per-block ramp avoids clicks
    bool first_block=true; // audio thread only; start at the pre-attachment target
};
static const char *ma_audio_gain_name(void *) { return "MA OBS – temporäre Audio-Regel"; }
static void *ma_audio_gain_create(obs_data_t *, obs_source_t *) {
    // Older collections may contain gain=0 from a previous cue. A restored
    // filter has no runtime owner and must pass audio until explicitly armed.
    return new MAControlGainFilter;
}
static void ma_audio_gain_save(void *, obs_data_t *settings) {
    // Saving must not alter the active atomic gain or the audio-thread ramp.
    obs_data_set_double(settings,"gain",1.0);
}
static void ma_audio_gain_destroy(void *data) { delete static_cast<MAControlGainFilter*>(data); }
static void ma_audio_gain_update(void *data, obs_data_t *settings) {
    static_cast<MAControlGainFilter*>(data)->gain.store(
        std::clamp(float(obs_data_get_double(settings,"gain")),0.0f,1.0f),
        std::memory_order_relaxed);
}
static struct obs_audio_data *ma_audio_gain_filter(void *data, struct obs_audio_data *audio) {
    auto *filter=static_cast<MAControlGainFilter*>(data);
    if (!audio) return audio;
    const float to=filter->gain.load(std::memory_order_relaxed);
    // A newly attached live input must not emit a unity-to-zero ramp before
    // its first silent/faded block. Restored filters still start neutral.
    if (filter->first_block) {filter->current_gain=to;filter->first_block=false;}
    const float from=filter->current_gain;
    if (to>=0.9999f && from>=0.9999f) return audio;
    // OBS audio filters receive float-planar samples. Linear smoothing over
    // one audio block avoids hard edges when MA fade values are updated.
    const float step=(to-from)/float(std::max(1u,audio->frames));
    for (size_t ch=0;ch<MAX_AV_PLANES;++ch) {
        if (!audio->data[ch]) continue;
        float *samples=reinterpret_cast<float*>(audio->data[ch]);
        float vol=from;
        for (uint32_t i=0;i<audio->frames;++i) {
            vol+=step;
            samples[i]*=vol;
        }
    }
    filter->current_gain=to;
    return audio;
}
static void ma_audio_remove_stale_filters(obs_source_t *source) {
    std::vector<obs_source_t*> stale;
    obs_source_enum_filters(source,[](obs_source_t *,obs_source_t *filter,void *data) {
        const char *id=obs_source_get_unversioned_id(filter);
        if (id && std::strcmp(id,"ma_obs_runtime_audio_gain")==0) {
            obs_source_t *ref=obs_source_get_ref(filter);
            if (ref) static_cast<std::vector<obs_source_t*>*>(data)->push_back(ref);
        }
    },&stale);
    for (auto *filter:stale) {
        obs_source_filter_remove(source,filter);
        obs_source_release(filter);
    }
}
struct MAControlGainBinding {
    obs_source_t *source=nullptr;
    obs_source_t *filter=nullptr;
    float current=1.0f;
};
class MAControlGains {
    std::map<QString,MAControlGainBinding> bindings; // UI thread only
public:
    void clear() {
        for (auto &kv:bindings) {
            auto &b=kv.second;
            if (b.source && b.filter) obs_source_filter_remove(b.source,b.filter);
            if (b.filter) obs_source_release(b.filter);
            if (b.source) obs_source_release(b.source);
        }
        bindings.clear();
        std::lock_guard<std::mutex> lock(ma_audio_policy_mutex);
        ma_audio_managed.clear();
    }
    void apply(const std::map<QString,float> &wanted) {
        for (auto it=bindings.begin();it!=bindings.end();) {
            if (wanted.count(it->first)) {++it;continue;}
            auto &b=it->second;
            if (b.source && b.filter) obs_source_filter_remove(b.source,b.filter);
            if (b.filter) obs_source_release(b.filter);
            if (b.source) obs_source_release(b.source);
            it=bindings.erase(it);
        }
        for (const auto &kv:wanted) {
            float gain=std::clamp(kv.second,0.0f,1.0f);
            auto it=bindings.find(kv.first);
            if (it==bindings.end()) {
                const auto uuid=kv.first.toUtf8();
                obs_source_t *source=obs_get_source_by_uuid(uuid.constData());
                if (!source) continue;
                ma_audio_remove_stale_filters(source);
                obs_data_t *settings=obs_data_create();
                obs_data_set_double(settings,"gain",gain);
                obs_source_t *filter=obs_source_create_private("ma_obs_runtime_audio_gain",
                    "MA OBS – temporäre Audio-Regel",settings);
                if (filter) obs_source_update(filter,settings); // arm before attachment
                obs_data_release(settings);
                if (!filter) {obs_source_release(source);continue;}
                obs_source_filter_add(source,filter);
                it=bindings.emplace(kv.first,MAControlGainBinding{source,filter,gain}).first;
            }
            if (std::abs(it->second.current-gain)>=0.004f) {
                obs_data_t *settings=obs_data_create();
                obs_data_set_double(settings,"gain",gain);
                obs_source_update(it->second.filter,settings);
                obs_data_release(settings);
                it->second.current=gain;
            }
        }
        std::set<std::string> managed;
        for (const auto &kv:bindings) managed.insert(kv.first.toUtf8().constData());
        std::lock_guard<std::mutex> lock(ma_audio_policy_mutex);
        ma_audio_managed=std::move(managed);
    }
};
static MAControlGains ma_audio_gains;
