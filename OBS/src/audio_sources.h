// v0.25c: collect each audible leaf once. Included inside plugin namespace.
#pragma once
struct MAAudioLeaf {
    obs_source_t *source = nullptr; // owned reference
    float weights[MAX_AUDIO_MIXES]{};
};
using MAAudioLeaves = std::map<obs_source_t *, MAAudioLeaf>;
static std::mutex ma_audio_policy_mutex;
static std::set<std::string> ma_audio_managed;

struct MAAudioWalk {
    MAAudioLeaves &leaves;
    const std::set<std::string> &managed;
    std::set<obs_source_t *> &path;
    obs_source_t *owner;
    float scene_weight;
    float parent_gain;
    uint32_t tracks;
};
static void ma_collect_audio(obs_source_t *, MAAudioWalk &);
static bool ma_collect_audio_item(obs_scene_t *, obs_sceneitem_t *item, void *data) {
    if (obs_sceneitem_visible(item))
        ma_collect_audio(obs_sceneitem_get_source(item), *static_cast<MAAudioWalk *>(data));
    return true;
}
static void ma_collect_audio(obs_source_t *source, MAAudioWalk &walk) {
    if (!source || source == walk.owner || walk.path.count(source)) return;
    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) scene = obs_group_from_source(source);
    if (scene) {
        if (obs_source_muted(source)) return;
        walk.path.insert(source);
        MAAudioWalk nested{walk.leaves, walk.managed, walk.path, walk.owner,
            walk.scene_weight, walk.parent_gain * obs_source_get_volume(source),
            walk.tracks & obs_source_get_audio_mixers(source)};
        obs_scene_enum_items(scene, ma_collect_audio_item, &nested);
        walk.path.erase(source);
        return;
    }
    if (!(obs_source_get_output_flags(source) & (OBS_SOURCE_AUDIO | OBS_SOURCE_COMPOSITE))) return;
    const char *uuid = obs_source_get_uuid(source);
    // Managed leaves already carry their individual gain filter. Never apply
    // scene crossfade a second time. Unmanaged audio follows scene weights.
    const bool managed = uuid && walk.managed.count(uuid);
    const float gain = walk.parent_gain * (managed ? 1.f : walk.scene_weight);
    auto &leaf = walk.leaves[source];
    if (!leaf.source) leaf.source = obs_source_get_ref(source);
    for (size_t m=0; m<MAX_AUDIO_MIXES; ++m)
        if (walk.tracks & (1u<<m)) leaf.weights[m] = std::max(leaf.weights[m], gain);
}
static void ma_release_audio_leaves(MAAudioLeaves &leaves) {
    for (auto &kv : leaves) obs_source_release(kv.second.source);
    leaves.clear();
}
