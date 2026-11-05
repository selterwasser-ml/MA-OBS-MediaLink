// MA OBS MediaLink v0.46 Video + Audio Control Beta - Qt/UI-thread orchestration for OBS media sources.
// Included exactly once from plugin.cpp, inside its anonymous namespace.
// Never retain OBS scene/source pointers across a collection switch.
#pragma once

struct MAVideoRule {
    bool enabled = false;
    QString audio_mode = "full"; // full, fade, end, mute
    QString start = "begin";   // begin, end, percent, delay, manual
    int percent = 50;
    int delay_ms = 2000;
    QString on_exit = "stop"; // stop, pause, continue
    QString reentry = "restart"; // restart, resume
    QString on_end = "hold"; // hold, black, loop
    QString before = "first"; // first, black
    QString on_abort = "continue"; // continue, stop, pause
    QString same_goto = "ignore"; // ignore, restart
    QString shared = "continue"; // continue, restart, pause_resume
    QString goto_mode = "immediate"; // immediate, normal
    int expected_fade_ms = 0; // optional declared cue fade duration
    int lead_before_end_ms = 1000; // start before planned fade end
    bool prepare_source = false; // explicit opt-in for editing native FFmpeg source settings
};
struct MAVideoEntry { QString uuid, name; bool ffmpeg = false, audio_only = false; };
struct MAVideoCue {
    bool valid = false;
    uint64_t event = 0;
    QString from_uuid, target_uuid;
    float progress = 0;
    bool reached = false;
    bool direct = false;
    std::vector<MALiveLayer> live_layers;
};
struct MAVideoPending {
    QString scene, source;
    MAVideoRule rule;
    bool fired = false;
    uint64_t last_loop_attempt_ms = 0;
    int64_t resume_ms = -1; // saved when leaving a previously playing video
    bool restore_hold_after_start = false; // temporary FFmpeg first-frame preload setting
    uint64_t last_preload_attempt_ms = 0;
    bool manual_override = false; // suppress automatic retrigger until the next cue
};
static QString ma_utf(const char *str) { return QString::fromUtf8(str ? str : ""); }
static std::string ma_videokey(const QString &s) { return s.toUtf8().toStdString(); }
static QString ma_vidt(const QString &s) { return s.isEmpty() ? QStringLiteral("–") : s; }

static bool ma_media_source(obs_source_t *s) {
    if (!s) return false;
    const char *id = obs_source_get_unversioned_id(s);
    return id && (strcmp(id, "ffmpeg_source") == 0 || strcmp(id, "vlc_source") == 0);
}
static bool ma_ffmpeg_source(obs_source_t *s) {
    if (!s) return false;
    const char *id = obs_source_get_unversioned_id(s);
    return id && strcmp(id, "ffmpeg_source") == 0;
}
static bool ma_blackmagic_audio_source(obs_source_t *s) {
    if (!s) return false;
    const char *id=obs_source_get_unversioned_id(s);
    return id && strcmp(id,"decklink-input")==0 &&
        (obs_source_get_output_flags(s)&OBS_SOURCE_AUDIO)!=0;
}
#include "source_settings.h"
// Recursive group/embedded-scene walk; deduplicate sources by UUID.
struct MAVideoWalkCtx { std::vector<MAVideoEntry> entries; std::set<QString> seen_sources; std::set<QString> walked_scenes; bool visible_only=false; };
static void ma_walk_media(obs_scene_t *scene, MAVideoWalkCtx &ctx, int depth);
static bool ma_video_scene_item(obs_scene_t *, obs_sceneitem_t *item, void *opaque) {
    auto *context = static_cast<std::pair<MAVideoWalkCtx *, int> *>(opaque);
    MAVideoWalkCtx &ctx = *context->first;
    if (ctx.visible_only && !obs_sceneitem_visible(item)) return true;
    obs_source_t *source = obs_sceneitem_get_source(item); // borrowed
    if (!source) return true;
    if (ma_media_source(source) || ma_blackmagic_audio_source(source)) {
        const QString uuid = ma_utf(obs_source_get_uuid(source));
        if (!uuid.isEmpty() && ctx.seen_sources.insert(uuid).second)
            ctx.entries.push_back({uuid, ma_utf(obs_source_get_name(source)), ma_ffmpeg_source(source),ma_blackmagic_audio_source(source)});
    }
    if (context->second < 8) {
        obs_scene_t *nested = obs_group_or_scene_from_source(source); // borrowed
        if (nested) ma_walk_media(nested, ctx, context->second + 1);
    }
    return true;
}
static void ma_walk_media(obs_scene_t *scene, MAVideoWalkCtx &ctx, int depth) {
    if (!scene || depth > 8) return;
    obs_source_t *root = obs_scene_get_source(scene); // borrowed
    if (ctx.visible_only && obs_source_muted(root)) return;
    const QString uuid = ma_utf(obs_source_get_uuid(root));
    if (!uuid.isEmpty() && !ctx.walked_scenes.insert(uuid).second) return;
    std::pair<MAVideoWalkCtx *, int> cb{&ctx, depth};
    obs_scene_enum_items(scene, ma_video_scene_item, &cb);
}
static std::vector<MAVideoEntry> ma_control_sources_in_scene(const QString &scene_uuid,bool visible_only=false) {
    MAVideoWalkCtx ctx;ctx.visible_only=visible_only;
    QByteArray bytes = scene_uuid.toUtf8();
    obs_source_t *root = obs_get_source_by_uuid(bytes.constData());
    if (root) {
        ma_walk_media(obs_scene_from_source(root), ctx, 0);
        obs_source_release(root);
    }
    return ctx.entries;
}
static std::vector<MAVideoEntry> ma_video_sources_in_scene(const QString &scene_uuid) {
    auto entries=ma_control_sources_in_scene(scene_uuid);
    entries.erase(std::remove_if(entries.begin(),entries.end(),[](const MAVideoEntry &entry){return entry.audio_only;}),entries.end());
    return entries;
}
static std::vector<std::pair<QString, QString>> ma_video_scenes() {
    std::vector<std::pair<QString, QString>> scenes;
    struct obs_frontend_source_list list{};
    obs_frontend_get_scenes(&list);
    for (size_t i = 0; i < list.sources.num; ++i) {
        obs_source_t *scene = list.sources.array[i];
        if (!scene) continue;
        const QString uuid = ma_utf(obs_source_get_uuid(scene));
        if (!uuid.isEmpty()) scenes.push_back({uuid, ma_utf(obs_source_get_name(scene))});
    }
    obs_frontend_source_list_free(&list);
    return scenes;
}
static QString ma_video_collection_name() {
    char *name = obs_frontend_get_current_scene_collection();
    QString result = ma_utf(name);
    if (name) bfree(name);
    return result;
}
static std::string ma_video_rules_path() {
    const QByteArray name = ma_video_collection_name().toUtf8();
    if (name.isEmpty()) return {};
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char c : name) { hash ^= c; hash *= UINT64_C(1099511628211); }
    char *dir = obs_module_config_path("video_rules");
    if (!dir) return {};
    os_mkdirs(dir);
    char suffix[24]{};
    snprintf(suffix, sizeof(suffix), "%016llx.json", (unsigned long long)hash);
    std::string path = std::string(dir) + "/" + suffix;
    bfree(dir);
    return path;
}
static MAVideoRule ma_video_rule_load(obs_data_t *d) {
    MAVideoRule r;
    r.enabled = obs_data_get_bool(d, "enabled");
    auto st = [d](const char *key, const QString &fallback) -> QString {
        const char *v = obs_data_get_string(d, key);
        return (v && *v) ? ma_utf(v) : fallback;
    };
    r.audio_mode = st("audio_mode", r.audio_mode);
    r.start = st("start", r.start);
    r.percent = int(obs_data_has_user_value(d, "percent") ? obs_data_get_int(d, "percent") : 50);
    r.delay_ms = int(obs_data_has_user_value(d, "delay_ms") ? obs_data_get_int(d, "delay_ms") : 2000);
    r.on_exit = st("on_exit", r.on_exit);
    r.reentry = st("reentry", r.reentry);
    r.on_end = st("on_end", r.on_end);
    r.before = st("before", r.before);
    r.on_abort = st("on_abort", r.on_abort);
    r.same_goto = st("same_goto", r.same_goto);
    r.shared = st("shared", r.shared);
    r.goto_mode = st("goto_mode", r.goto_mode);
    r.expected_fade_ms = int(obs_data_get_int(d, "expected_fade_ms"));
    r.lead_before_end_ms = int(obs_data_has_user_value(d, "lead_before_end_ms") ? obs_data_get_int(d, "lead_before_end_ms") : 1000);
    r.prepare_source = obs_data_get_bool(d, "prepare_source");
    return r;
}
static obs_data_t *ma_video_rule_save(const MAVideoRule &r) {
    obs_data_t *d = obs_data_create();
    obs_data_set_bool(d, "enabled", r.enabled);
    auto st = [d](const char *key, const QString &s) { auto utf = s.toUtf8(); obs_data_set_string(d, key, utf.constData()); };
    st("start", r.start); st("audio_mode", r.audio_mode);
    obs_data_set_int(d, "percent", r.percent);
    obs_data_set_int(d, "delay_ms", r.delay_ms);
    st("on_exit", r.on_exit); st("reentry", r.reentry); st("on_end", r.on_end);
    st("before", r.before); st("on_abort", r.on_abort); st("same_goto", r.same_goto);
    st("shared", r.shared); st("goto_mode", r.goto_mode);
    obs_data_set_int(d, "expected_fade_ms", r.expected_fade_ms);
    obs_data_set_int(d, "lead_before_end_ms", r.lead_before_end_ms);
    obs_data_set_bool(d, "prepare_source", r.prepare_source);
    return d;
}
static QString ma_video_media_state(enum obs_media_state state) {
    switch (state) {
    case OBS_MEDIA_STATE_PLAYING: return "Läuft";
    case OBS_MEDIA_STATE_PAUSED: return "Pause";
    case OBS_MEDIA_STATE_STOPPED: return "Stopp";
    case OBS_MEDIA_STATE_ENDED: return "Ende";
    case OBS_MEDIA_STATE_OPENING: return "Öffnet";
    case OBS_MEDIA_STATE_BUFFERING: return "Puffer";
    case OBS_MEDIA_STATE_ERROR: return "Fehler";
    default: return "Bereit";
    }
}
static QString ma_video_ms(int64_t ms) {
    if (ms < 0) ms = 0;
    const int64_t sec = ms / 1000;
    return QString("%1:%2:%3")
        .arg(sec/3600, 2, 10, QChar('0')).arg((sec/60)%60, 2, 10, QChar('0'))
        .arg(sec%60, 2, 10, QChar('0'));
}
static bool ma_video_apply_ffmpeg(obs_source_t *source, const MAVideoRule &r) {
    if (!ma_media_control_active() || !ma_ffmpeg_source(source)) return false;
    obs_data_t *settings = obs_source_get_settings(source);
    if (!settings) return false;
    if (!ma_remember_media_settings(source,settings)) {
        obs_data_release(settings);
        blog(LOG_WARNING,"MA OBS: source preparation skipped: original settings unavailable");
        return false;
    }
    // An explicit per-rule opt-in: borrows native OBS source properties.
    // Updating an active OBS Media Source is NOT a harmless setter: FFmpeg
    // can restart playback during an update (especially at ENDED + loop).
    // Only update if a persisted property really differs, and only when a
    // configured media cue is prepared; never on Settings -> Save.
    bool changed = false;
    auto set_changed = [&](const char *key, bool value) {
        if (obs_data_get_bool(settings, key) != value) {
            obs_data_set_bool(settings, key, value);
            changed = true;
        }
    };
    set_changed("restart_on_activate", false);
    set_changed("close_when_inactive", false);
    set_changed("looping", r.on_end == "loop");
    // FFmpeg only preloads a frame when clear_on_media_end OR looping is on.
    // Temporarily enable that flag for first-frame preparation; on the actual
    // trigger we restore 'hold last frame' without restarting the decoder.
    set_changed("clear_on_media_end", r.on_end == "black" || r.before == "first");
    if (changed) obs_source_update(source, settings);
    obs_data_release(settings);
    return changed;
}
static void ma_video_restore_hold(obs_source_t *source) {
    if (!ma_media_control_active() || !ma_ffmpeg_source(source)) return;
    obs_data_t *settings=obs_source_get_settings(source);
    if (!settings) return;
    if (!ma_remember_media_settings(source,settings)) {obs_data_release(settings);return;}
    if (obs_data_get_bool(settings,"clear_on_media_end")) {
        obs_data_set_bool(settings,"clear_on_media_end",false);
        obs_source_update(source,settings);
    }
    obs_data_release(settings);
}
#include "media_actions.h"
static bool ma_video_command(const QString &uuid, const QString &operation) {
    if (!ma_media_control_active()) return false;
    const QByteArray bytes=uuid.toUtf8();
    obs_source_t *source=obs_get_source_by_uuid(bytes.constData());
    if (!source) return false;
    bool handled=false;
    if (ma_media_source(source)) {
        if (operation=="first") {
            obs_source_media_stop(source);
            obs_source_media_set_time(source,0);
            if (ma_ffmpeg_source(source)) {
                proc_handler_t *handler=obs_source_get_proc_handler(source);
                if (handler) {
                    calldata_t params;
                    calldata_init(&params);
                    proc_handler_call(handler,"preload_first_frame",&params);
                    calldata_free(&params);
                    obs_source_show_preloaded_video(source);
                }
            }
            handled=true;
        } else {
            MAMediaIntent intent=MAMediaIntent::Play;
            handled=true;
            if (operation=="toggle") intent=MAMediaIntent::Toggle;
            else if (operation=="restart") intent=MAMediaIntent::Restart;
            else if (operation=="pause") intent=MAMediaIntent::Pause;
            else if (operation=="stop") intent=MAMediaIntent::Stop;
            else if (operation!="play") handled=false;
            if (handled) ma_execute_media_command(source,
                ma_resolve_media_command(intent,obs_source_media_get_state(source)));
        }
    }
    obs_source_release(source);
    return handled;
}

struct MAVideoEngine {
    bool enabled = false;
    bool suspended = false;
    QString collection;
    std::map<QString, std::map<QString, MAVideoRule>> rules;
    // Runtime membership uses only saved rules; editor drafts are not published.
    std::map<QString, std::map<QString, MAVideoRule>> committed_rules;
    uint64_t last_membership_ms=0;
    uint64_t cue_event = 0;
    QString cue_scene;
    float last_progress = 1;
    uint64_t event_started_ms = 0;
    bool got_event = false;
    maobs::LiveAudioPolicy live_audio_policy;
    std::map<QString,float> live_audio_gains;
    std::map<QString,float> media_fade_weights;
    std::map<QString,MAVideoPending> continued_audio;
    std::vector<MAVideoPending> pending;   // incoming / current scene
    std::vector<MAVideoPending> outgoing;  // still visible until fade completes
    std::map<QString, int64_t> resume_positions;
    std::set<QString> continued_sources; // explicitly allowed to play across scene changes
    std::set<QString> forced_showing; // own OBS showing counters; NO stored source pointers
    MAVideoCue last_cue;

    MAVideoRule get(const QString &scene, const QString &source) const {
        auto s=rules.find(scene);
        if (s==rules.end()) return {};
        auto r=s->second.find(source);
        return r==s->second.end() ? MAVideoRule{} : r->second;
    }
    MAVideoRule get_committed(const QString &scene,const QString &source) const {
        auto s=committed_rules.find(scene);
        if (s==committed_rules.end()) return {};
        auto r=s->second.find(source);
        return r==s->second.end() ? MAVideoRule{} : r->second;
    }
    void set(const QString &scene, const QString &source, const MAVideoRule &r) {
        rules[scene][source]=r;
    }
    void show_source(const QString &uuid) {
        if (!forced_showing.insert(uuid).second) return;
        const QByteArray id=uuid.toUtf8();
        obs_source_t *source=obs_get_source_by_uuid(id.constData());
        if (source) { obs_source_inc_showing(source); obs_source_release(source); }
        else forced_showing.erase(uuid);
    }
    void release_source(const QString &uuid) {
        auto it=forced_showing.find(uuid);
        if (it==forced_showing.end()) return;
        const QByteArray id=uuid.toUtf8();
        obs_source_t *source=obs_get_source_by_uuid(id.constData());
        if (source) { obs_source_dec_showing(source); obs_source_release(source); }
        forced_showing.erase(it);
    }
    void reset_runtime() {
        for (const auto &uuid:forced_showing) {
            const QByteArray id=uuid.toUtf8();
            obs_source_t *source=obs_get_source_by_uuid(id.constData());
            if (source) { obs_source_dec_showing(source); obs_source_release(source); }
        }
        forced_showing.clear();
        resume_positions.clear();
        continued_sources.clear();
        continued_audio.clear();media_fade_weights.clear();
        last_membership_ms=0;
        cue_event=0; cue_scene.clear(); got_event=false;
        pending.clear(); outgoing.clear(); last_cue={}; last_progress=1;
        live_audio_policy.clear();live_audio_gains.clear();
    }
    void load() {
        reset_runtime(); rules.clear(); committed_rules.clear(); enabled=false;
        collection=ma_video_collection_name();
        const auto path=ma_video_rules_path();
        obs_data_t *root=path.empty()?nullptr:obs_data_create_from_json_file_safe(path.c_str(), "bak");
        if (!root) return;
        enabled=obs_data_get_bool(root,"video_enabled");
        obs_data_array_t *arr=obs_data_get_array(root,"rules");
        if (arr) {
            const size_t count=obs_data_array_count(arr);
            for (size_t i=0;i<count;++i) {
                obs_data_t *item=obs_data_array_item(arr,i);
                const QString scene=ma_utf(obs_data_get_string(item,"scene_uuid"));
                const QString source=ma_utf(obs_data_get_string(item,"source_uuid"));
                if (!scene.isEmpty() && !source.isEmpty()) rules[scene][source]=ma_video_rule_load(item);
                obs_data_release(item);
            }
            obs_data_array_release(arr);
        }
        obs_data_release(root);
        committed_rules=rules;
        blog(LOG_INFO,"MA OBS Video: loaded %zu scene rule sets (enabled=%d)",rules.size(),enabled?1:0);
    }
    bool save() {
        const auto path=ma_video_rules_path();
        if (path.empty()) return false;
        obs_data_t *root=obs_data_create();
        auto col=collection.toUtf8();
        obs_data_set_string(root,"collection",col.constData());
        obs_data_set_bool(root,"video_enabled",enabled);
        obs_data_array_t *arr=obs_data_array_create();
        for (const auto &scene:rules) for (const auto &it:scene.second) {
            obs_data_t *r=ma_video_rule_save(it.second);
            auto sid=scene.first.toUtf8(), mid=it.first.toUtf8();
            obs_data_set_string(r,"scene_uuid",sid.constData());
            obs_data_set_string(r,"source_uuid",mid.constData());
            obs_data_array_push_back(arr,r); obs_data_release(r);
        }
        obs_data_set_array(root,"rules",arr);
        const bool ok=obs_data_save_json_safe(root,path.c_str(),"tmp","bak");
        obs_data_array_release(arr); obs_data_release(root);
        if (ok) {committed_rules=rules; last_membership_ms=0;}
        else blog(LOG_WARNING,"MA OBS Video: rules save failed");
        return ok;
    }
    bool shared_in_scene(const QString &scene,const QString &uuid) const {
        for (const auto &entry:ma_video_sources_in_scene(scene))
            if (entry.uuid == uuid) return true;
        return false;
    }
    void exit_action(const MAVideoPending &p,const QString &mode) {
        if (mode=="stop" || mode=="pause") {
            continued_sources.erase(p.source);
            continued_audio.erase(p.source);
            const QByteArray id=p.source.toUtf8();
            obs_source_t *source=obs_get_source_by_uuid(id.constData());
            if (source) {
                const int64_t time=obs_source_media_get_time(source);
                if (time>=0) resume_positions[p.source]=time;
                obs_source_release(source);
            }
            ma_video_command(p.source,mode);
            release_source(p.source);
        } else if (mode=="continue") {
            continued_sources.insert(p.source);
            continued_audio[p.source]=p;
        }
        // continue deliberately keeps its independent showing counter so the
        // decoder is not deactivated merely because the scene became invisible.
        // Collection unload / pilot stop releases ALL these counters.
    }
    void finish_outgoing(const QString &new_scene = QString(), bool interrupted=false) {
        for (const auto &p:outgoing) {
            if (!new_scene.isEmpty() && shared_in_scene(new_scene,p.source)) continue;
            exit_action(p,interrupted ? p.rule.on_abort : p.rule.on_exit);
        }
        outgoing.clear();
    }
    void stop_pilot() {
        // Hand control back to OBS. Exit rules belong to MA scene changes;
        // disabling control must not issue another Stop/Pause/Restart command.
        ma_audio_gains.clear();
        reset_runtime();
        ma_restore_media_sources();
    }
    void change_collection(bool pause) {
        suspended=pause;
        if (pause) stop_pilot();
        else reset_runtime();
        if (!pause) load();
    }
    void add_pending(const MAVideoCue &cue,const MAVideoEntry &media,
        const MAVideoRule &rule,const QString &previous_scene) {
        MAVideoPending p{cue.target_uuid,media.uuid,rule,false};
        p.restore_hold_after_start=rule.prepare_source && rule.before=="first" &&
            rule.on_end=="hold";
        const bool was_continuing=continued_sources.erase(media.uuid)>0;
        continued_audio.erase(media.uuid);
        bool in_outgoing=false;
        for (const auto &other:outgoing) if (other.source==media.uuid) in_outgoing=true;
        const bool truly_shared = was_continuing || in_outgoing ||
            (!previous_scene.isEmpty() && shared_in_scene(previous_scene,media.uuid));
        const auto id=media.uuid.toUtf8();
        obs_source_t *source=obs_get_source_by_uuid(id.constData());
        if (source) {
            // Prepare settings BEFORE inc_showing(): OBS activation can
            // restart local video sources if restart_on_activate is still ON.
            if (rule.prepare_source) ma_video_apply_ffmpeg(source,rule);
            obs_source_release(source);
        }
        show_source(media.uuid);
        source=obs_get_source_by_uuid(id.constData());
        if (source) {
            const auto status=obs_source_media_get_state(source);
            const bool playing=status==OBS_MEDIA_STATE_PLAYING ||
                status==OBS_MEDIA_STATE_BUFFERING;
            auto saved=resume_positions.find(media.uuid);
            if (saved!=resume_positions.end()) p.resume_ms=saved->second;
            if (truly_shared && playing && rule.shared=="continue") {
                p.fired=true;
            } else if (truly_shared && playing && rule.shared=="pause_resume") {
                obs_source_media_play_pause(source,true);
            } else if (rule.start!="manual") {
                // A source may be playing because OBS Preview or an auto-
                // activation started it. This is NOT a MA start trigger.
                // Enforce the user's time/percentage trigger first.
                const bool preserve_pause=rule.reentry=="resume" &&
                    status==OBS_MEDIA_STATE_PAUSED;
                const bool preserve_time=rule.reentry=="resume" && p.resume_ms>=0;
                if (!preserve_pause) {
                    if (rule.before=="first" && !preserve_time)
                        ma_video_command(media.uuid,"first");
                    else if (!preserve_time)
                        obs_source_media_stop(source);
                    else if (playing)
                        obs_source_media_play_pause(source,true);
                }
            }
            obs_source_release(source);
        }
        pending.push_back(p);
    }
    void sync_scene_membership(const MAVideoCue &cue) {
        const auto sources=ma_video_sources_in_scene(cue.target_uuid);
        std::map<QString,MAVideoRule> eligible;
        for (const auto &media:sources) {
            const auto rule=get_committed(cue.target_uuid,media.uuid);
            if (rule.enabled) eligible.emplace(media.uuid,rule);
        }
        for (auto it=pending.begin();it!=pending.end();) {
            if (eligible.count(it->source)) {++it;continue;}
            const QString uuid=it->source;
            it=pending.erase(it);
            // Removing a scene item must not stop another scene's shared decoder.
            bool retained=continued_sources.count(uuid)>0;
            for (const auto &other:outgoing) if (other.source==uuid) retained=true;
            if (!retained) release_source(uuid);
        }
        std::set<QString> existing;
        for (const auto &p:pending) existing.insert(p.source);
        for (const auto &media:sources) {
            const auto rule=eligible.find(media.uuid);
            if (rule!=eligible.end() && existing.insert(media.uuid).second)
                add_pending(cue,media,rule->second,QString());
        }
    }
    void enter(const MAVideoCue &cue,uint64_t now) {
        const bool repeat_same=got_event && cue_scene==cue.target_uuid;
        if (repeat_same) {
            // A repeated Goto applies to EACH individual video. Do not run
            // enter() again: that would preload/rewind all videos regardless
            // of their 'ignore' choice or the shared-source policy.
            for (auto &p:pending) if (p.rule.start!="manual" && p.rule.same_goto=="restart") {
                show_source(p.source);
                ma_video_command(p.source,"restart");
                p.manual_override=false;
                p.fired=true;
                resume_positions.erase(p.source);
                blog(LOG_INFO,"MA OBS Video: same-scene Goto restarts %s",
                    p.source.toUtf8().constData());
            }
            cue_event=cue.event;
            return;
        }

        // A previous outgoing scene may have been frozen for an interrupted
        // fade. Resolve its exit rules before admitting yet another cue.
        const bool was_aborted=got_event && last_progress<0.999f;
        finish_outgoing(cue.target_uuid,was_aborted);
        std::vector<MAVideoPending> previous=std::move(pending);
        pending.clear();
        const QString previous_scene=cue_scene;
        for (const auto &p:previous) {
            if (shared_in_scene(cue.target_uuid,p.source)) continue;
            if (was_aborted && p.rule.on_abort!="continue")
                exit_action(p,p.rule.on_abort);
            else
                outgoing.push_back(p); // fade-out remains visible until 100%
        }
        cue_event=cue.event;
        cue_scene=cue.target_uuid;
        event_started_ms=now;
        got_event=true;
        const auto sources=ma_video_sources_in_scene(cue.target_uuid);
        for (const auto &media:sources) {
            const auto rule=get_committed(cue.target_uuid,media.uuid);
            if (rule.enabled) add_pending(cue,media,rule,previous_scene);
        }
        last_membership_ms=now;
        blog(LOG_INFO,"MA OBS Video: new cue=%llu target=%s pending=%zu outgoing=%zu",
            (unsigned long long)cue.event,cue.target_uuid.toUtf8().constData(),
            pending.size(),outgoing.size());
    }
    void update_live_audio(const MAVideoCue &cue) {
        live_audio_gains.clear();
        media_fade_weights.clear();
        if (!ma_media_control_active() || !enabled || suspended || !cue.valid) return;
        auto layers=cue.live_layers;
        bool target_present=false;
        for (const auto &layer:layers) if (ma_utf(layer.uuid.c_str())==cue.target_uuid) target_present=true;
        if (!target_present && !cue.target_uuid.isEmpty())
            layers.push_back({ma_videokey(cue.target_uuid),"",0.f});
        std::vector<maobs::LiveAudioItem> items;
        for (const auto &layer:layers) {
            const QString scene=ma_utf(layer.uuid.c_str());
            for (const auto &entry:ma_control_sources_in_scene(scene,true)) {
                if (!entry.audio_only) {
                    // One physical decoder can be used in several live layers.
                    // Retargeting keeps the old layers' actual fractions;
                    // never restart its fade-out from 1 - new_cue_progress.
                    media_fade_weights[entry.uuid]=std::min(1.f,
                        media_fade_weights[entry.uuid]+std::clamp(layer.weight,0.f,1.f));
                    continue;
                }
                const auto rule=get_committed(scene,entry.uuid);
                items.push_back({ma_videokey(entry.uuid),layer.uuid,layer.weight,
                    rule.enabled,ma_videokey(rule.audio_mode)});
            }
        }
        const auto gains=live_audio_policy.gains(items,ma_videokey(cue.target_uuid),
            cue.reached || cue.progress>=.999f || cue.direct);
        for (const auto &gain:gains) live_audio_gains.emplace(ma_utf(gain.first.c_str()),gain.second);
    }
    std::map<QString,float> audio_gains() const {
        std::map<QString,float> result;
        if (!enabled || suspended || !got_event) return result;
        result=live_audio_gains;
        const float progress=std::clamp(last_cue.progress,0.f,1.f);
        auto gain_for=[&](const MAVideoPending &p)->float {
            const QString &mode=p.rule.audio_mode;
            if (mode=="mute" || !p.fired) return 0.f;
            if (mode=="end") return progress>=0.999f ? 1.f : 0.f;
            // Individual fade is applied exactly once; mixer bypasses scene gain.
            // OBS's normal per-source fader and track routing are preserved.
            if (mode=="fade") {
                const auto found=media_fade_weights.find(p.source);
                return found==media_fade_weights.end()?0.f:found->second;
            }
            return 1.f;
        };
        for (const auto &p:outgoing) result[p.source]=gain_for(p);
        for (const auto &p:pending) result[p.source]=gain_for(p);
        for (const auto &uuid:continued_sources) if (!result.count(uuid)) {
            const auto previous=continued_audio.find(uuid);
            if (previous==continued_audio.end()) {result[uuid]=0.f;continue;}
            const auto &p=previous->second;
            result[uuid]=(p.rule.audio_mode=="fade" || p.rule.audio_mode=="mute" || !p.fired)
                ? gain_for(p):1.f;
        }
        return result;
    }
    void manual(const QString &uuid,const QString &operation) {
        if (!ma_media_control_active() || !enabled || suspended) return;
        QString command=operation;
        if (command=="toggle") {
            const auto bytes=uuid.toUtf8();
            obs_source_t *source=obs_get_source_by_uuid(bytes.constData());
            if (!source) return;
            command=ma_media_can_pause(obs_source_media_get_state(source)) ? "pause" : "play";
            obs_source_release(source);
        }
        if (!ma_video_command(uuid,command)) return;
        for (auto &p:pending) if (p.source==uuid) {
            // Pause/Stop must not re-arm the start rule on the next 50 ms tick.
            p.manual_override=true;
            p.fired=command=="play" || command=="restart";
            if (p.fired) {
                resume_positions.erase(p.source);
                if (p.restore_hold_after_start) {
                    const auto bytes=p.source.toUtf8();
                    obs_source_t *source=obs_get_source_by_uuid(bytes.constData());
                    if (source) {ma_video_restore_hold(source); obs_source_release(source);}
                    p.restore_hold_after_start=false;
                }
            }
        }
    }
    void tick(const MAVideoCue &cue) {
        last_cue=cue;
        if (!ma_media_control_active() || suspended || !enabled || !cue.valid || cue.target_uuid.isEmpty()) return;
        const uint64_t now=GetTickCount64();
        if (!got_event || cue.event!=cue_event || cue.target_uuid!=cue_scene) enter(cue,now);
        last_progress=cue.progress;
        if (!last_membership_ms || now-last_membership_ms>=250) {
            sync_scene_membership(cue);
            last_membership_ms=now;
        }
        for (auto &p:pending) {
            if (p.fired && p.rule.on_end=="loop") {
                const QByteArray id=p.source.toUtf8();
                obs_source_t *source=obs_get_source_by_uuid(id.constData());
                if (source) {
                    if (!p.rule.prepare_source || !ma_ffmpeg_source(source)) {
                        if (obs_source_media_get_state(source)==OBS_MEDIA_STATE_ENDED &&
                            now-p.last_loop_attempt_ms>=1000) {
                            obs_source_media_restart(source);
                            p.last_loop_attempt_ms=now;
                        }
                    }
                    obs_source_release(source);
                }
            }
            if (!ma_media_automatic_start_allowed(p.fired,p.manual_override,p.rule.start=="manual")) continue;
            // The decoder may deliver a preloaded frame slightly after the
            // synchronous preload request. Show it again once available,
            // without restarting playback or touching the time position.
            if (p.rule.before=="first" && p.rule.prepare_source &&
                now-p.last_preload_attempt_ms>=150) {
                const QByteArray id=p.source.toUtf8();
                obs_source_t *source=obs_get_source_by_uuid(id.constData());
                if (source && ma_ffmpeg_source(source))
                    obs_source_show_preloaded_video(source);
                if (source) obs_source_release(source);
                p.last_preload_attempt_ms=now;
            }
            bool due=false;
            if (p.rule.goto_mode=="immediate" && cue.direct) due=true;
            else if (p.rule.start=="begin") due=true;
            else if (p.rule.start=="end") due=cue.progress>=0.999f || cue.reached;
            else if (p.rule.start=="percent") due=cue.progress*100.0f+0.05f>=p.rule.percent;
            else if (p.rule.start=="delay") due=now-event_started_ms>=uint64_t(p.rule.delay_ms);
            else if (p.rule.start=="before_end") {
                if (p.rule.expected_fade_ms>0)
                    due=now-event_started_ms>=uint64_t(std::max(0,p.rule.expected_fade_ms-p.rule.lead_before_end_ms));
                else due=cue.reached;
            }
            if (!due) continue;
            const QByteArray id=p.source.toUtf8();
            obs_source_t *source=obs_get_source_by_uuid(id.constData());
            if (source) {
                const auto state=obs_source_media_get_state(source);
                if (p.rule.reentry=="resume" && state==OBS_MEDIA_STATE_PAUSED) {
                    obs_source_media_play_pause(source,false);
                } else {
                    obs_source_media_restart(source);
                    if (p.rule.reentry=="resume" && p.resume_ms>0)
                        obs_source_media_set_time(source,p.resume_ms);
                }
                if (p.restore_hold_after_start) {
                    // This source update only toggles clear_on_media_end; OBS
                    // FFmpeg doesn't restart active local playback for it.
                    ma_video_restore_hold(source);
                    p.restore_hold_after_start=false;
                }
                obs_source_release(source);
            }
            p.fired=true;
            resume_positions.erase(p.source);
            blog(LOG_INFO,"MA OBS Video: trigger source=%s mode=%s progress=%.1f%% delay=%llu",
                 p.source.toUtf8().constData(),p.rule.start.toUtf8().constData(),
                 cue.progress*100.f,(unsigned long long)(now-event_started_ms));
        }
        if (cue.reached || cue.progress>=0.999f || cue.direct)
            finish_outgoing(cue.target_uuid);
    }
};
static MAVideoEngine ma_video;

#include "about_ui.h"
static QPointer<QDialog> ma_settings_window;
static void ma_settings_dialog(int initial_tab) {
    if (ma_obs_exiting) return;
    if (ma_settings_window) {
        if (initial_tab>=0) {
            auto *existing=ma_settings_window->findChild<QTabWidget *>("medialink_settings_tabs");
            if (existing) existing->setCurrentIndex(initial_tab);
        }
        ma_settings_window->show(); ma_settings_window->raise(); ma_settings_window->activateWindow();
        return;
    }
    if (ma_video.suspended) return;
    const auto saved_rules=ma_video.rules;
    const bool saved_enabled=ma_video.enabled;
    const QString original_collection=ma_video_collection_name();
    const uint64_t original_epoch=ma_current_collection_epoch();
    QSettings view(ma_ui_state_path(),QSettings::IniFormat);
    view.beginGroup("collections/"+QString::fromLatin1(original_collection.toUtf8().toHex()));
    QDialog dialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
    dialog.setWindowIcon(QIcon(QStringLiteral(":/ma-obs-medialink/medialink-logo.png")));
    ma_settings_window=&dialog;
    dialog.setWindowTitle("MA OBS MediaLink · v0.46 BETA – Einstellungen");
    dialog.resize(1050,720); dialog.setMinimumSize(760,480);
    const QByteArray geometry=view.value("geometry").toByteArray();
    if (!geometry.isEmpty()) dialog.restoreGeometry(geometry);
    auto *window_layout=new QVBoxLayout(&dialog);
    window_layout->addWidget(new QLabel("MA OBS MediaLink · v0.46 BETA",&dialog));
    auto *tabs=new QTabWidget(&dialog); tabs->setObjectName("medialink_settings_tabs");
    window_layout->addWidget(tabs,1);
    std::function<bool()> save_native;
    ma_add_native_pages(&dialog,tabs,save_native);
    auto *video_page=new QWidget(tabs);
    tabs->addTab(video_page,"Video und Audio");
    auto *layout=new QVBoxLayout(video_page);
    auto *top=new QHBoxLayout;
    auto *global=new QCheckBox("Video- und Audiosteuerung für diese Szenensammlung aktivieren", &dialog);
    global->setChecked(ma_video.enabled);
    top->addWidget(global); top->addStretch();
    auto *reload=new QPushButton("Szenen/Quellen neu einlesen", &dialog);
    top->addWidget(reload); layout->addLayout(top);
    auto *description=new QLabel("Links die OBS-Szene, daneben die Medien- oder Blackmagic-Quelle auswählen. Regeln werden pro Kombination aus Szene und Quelle gespeichert.\n"
        "Nur aktivierte Regeln werden angewandt. Beim Deaktivieren werden die ursprünglichen OBS-Quelleneinstellungen wiederhergestellt.", &dialog);
    description->setWordWrap(true); layout->addWidget(description);
    auto *inactive_notice=new QLabel(video_page);inactive_notice->setWordWrap(true);layout->addWidget(inactive_notice);
    const auto update_inactive_notice=[=] {
        QStringList messages;
        if (!pilot_ui_active.load()) messages.append("MA-Steuerung ist ausgeschaltet.");
        if (!global->isChecked()) messages.append("Video- und Audiosteuerung für diese Szenensammlung ist ausgeschaltet.");
        inactive_notice->setText(messages.isEmpty()?QString():messages.join(" ")+" Gespeicherte Regeln können weiterhin bearbeitet werden.");
        inactive_notice->setVisible(!messages.isEmpty());
    };
    auto *notice_timer=new QTimer(&dialog);
    QObject::connect(notice_timer,&QTimer::timeout,&dialog,update_inactive_notice);notice_timer->start(250);
    QObject::connect(global,&QCheckBox::toggled,&dialog,[=](bool) {update_inactive_notice();});
    update_inactive_notice();
    auto *splitter=new QSplitter(Qt::Horizontal,&dialog);
    auto *scenes=new QListWidget(splitter); scenes->setMinimumWidth(190);
    auto *media=new QListWidget(splitter); media->setMinimumWidth(195);
    auto *scroll=new QScrollArea(splitter); scroll->setWidgetResizable(true);
    auto *formWidget=new QWidget(scroll);
    auto *form=new QVBoxLayout(formWidget);
    auto *selected=new QLabel("Keine Medienquelle gewählt",formWidget);
    selected->setWordWrap(true); selected->setStyleSheet("font-weight:bold;"); form->addWidget(selected);
    auto *enabled=new QCheckBox("Medienquelle in dieser Szene steuern",formWidget); form->addWidget(enabled);
    auto *options=new QFormLayout;
    auto *start=new QComboBox(formWidget);
    start->addItem("Bei Fadebeginn", "begin"); start->addItem("Bei Fadeende (100 %)", "end");
    start->addItem("Bei X % Fadefortschritt", "percent"); start->addItem("X Sekunden nach Fadebeginn", "delay");
    start->addItem("X Sekunden vor geplantem Fadeende", "before_end");
    start->addItem("Nur manuell", "manual"); options->addRow("Videostart", start);
    auto *percent=new QSpinBox(formWidget); percent->setRange(0,100); percent->setSuffix(" %"); options->addRow("Startschwelle",percent);
    auto *delay=new QDoubleSpinBox(formWidget); delay->setRange(0,3600); delay->setDecimals(2); delay->setSuffix(" s"); options->addRow("Verzögerung",delay);
    auto *expected=new QDoubleSpinBox(formWidget); expected->setRange(0,3600); expected->setDecimals(2);
    expected->setSpecialValueText("Unbekannt → bei tatsächlichem Fadeende"); expected->setSuffix(" s");
    options->addRow("Geplante Fadedauer",expected);
    auto *before_end=new QDoubleSpinBox(formWidget); before_end->setRange(0,3600);
    before_end->setDecimals(2); before_end->setSuffix(" s");
    options->addRow("Start vor geplantem Ende",before_end);
    auto combo=[&](const QString &title,std::initializer_list<std::pair<const char*,const char*>> values) {
        auto *cb=new QComboBox(formWidget);
        for (auto &value:values) cb->addItem(QString::fromUtf8(value.first),QString::fromUtf8(value.second));
        options->addRow(title,cb); return cb;
    };
    auto *audio_mode=combo("Video-Ton",{{"OBS Mix / Ab Videostart (ohne MA-Fade)","full"},
        {"Mit MA-Fade","fade"},
        {"Ab Fadeende (100 %)","end"},
        {"Immer stumm","mute"}});
    audio_mode->setToolTip("OBS Mix behält den OBS-Pegel unabhängig vom MA-Fade. "
        "Mit MA-Fade regelt jede Medienquelle genau einmal. "
        "Mehrfach verwendete OBS-Medienquellen teilen einen einzigen Audio-Decoder.");
    auto *on_exit=combo("Beim Verlassen",{{"Stoppen","stop"},{"Pausieren","pause"},{"Weiterlaufen","continue"}});
    auto *reentry=combo("Bei erneutem Aufruf",{{"Von vorne","restart"},{"Fortsetzen","resume"}});
    auto *on_end=combo("Am Videoende",{{"Letztes Bild halten","hold"},{"Schwarz / leer","black"},{"Wiederholen (Loop)","loop"}});
    auto *on_end_basic=combo("Am Videoende",{{"OBS-Quelleneinstellungen verwenden","obs"},{"Wiederholen (Loop)","loop"}});
    auto *before=combo("Vor dem Start",{{"Erstes Videobild","first"},{"Schwarz / leer","black"}});
    auto *on_abort=combo("Abgebrochener Fade",{{"Weiterlaufen","continue"},{"Stoppen","stop"},{"Pausieren","pause"}});
    auto *same_goto=combo("Goto auf gleiche Szene",{{"Ignorieren","ignore"},{"Video neu starten","restart"}});
    auto *shared=combo("Quelle läuft bereits",{{"Weiterlaufen","continue"},{"Neustarten","restart"},{"Pausieren, dann fortsetzen","pause_resume"}});
    auto *goto_mode=combo("Direkter Sprung",{{"Sofort auslösen","immediate"},{"Normale Startregel","normal"}});
    const std::vector<QWidget *> video_controls={start,percent,delay,expected,before_end,
        on_exit,reentry,on_end,on_end_basic,before,on_abort,same_goto,shared,goto_mode};
    form->addLayout(options);
    auto *timing_note=new QLabel(formWidget);timing_note->setWordWrap(true);form->addWidget(timing_note);
    auto *timing_error=new QLabel("Der Startabstand vor dem Fadeende darf nicht größer als die geplante Fadedauer sein.",formWidget);
    timing_error->setWordWrap(true);timing_error->setStyleSheet("color:#e6a23c;");form->addWidget(timing_error);
    auto *display_info=new QLabel("Die Bilddarstellung vor dem Start und am Videoende wird durch die OBS-Quelleneinstellungen bestimmt. Wiederholen kann MediaLink auslösen.",formWidget);
    display_info->setWordWrap(true);form->addWidget(display_info);
    auto *prepare=new QCheckBox("OBS-Quelle automatisch für MA vorbereiten (FFmpeg)",formWidget);
    prepare->setToolTip("Deaktiviert 'Beim Aktivieren neu starten' und 'Datei bei Inaktivität schließen'. "
        "Ändert Loop und 'Nach Videoende nichts anzeigen' entsprechend der Videoregel. "
        "Die Änderung wird erst beim nächsten MA-Szenenaufruf angewandt. "
        "Beim Deaktivieren der MA-Steuerung werden die ursprünglichen Optionen wiederhergestellt.");
    form->addWidget(prepare);
    auto *prepare_info=new QLabel("Wichtig: Ohne Vorbereitung kann OBS eine Medienquelle bereits vor der Fade-Schwelle automatisch starten. "
        "Bei VLC-Quellen sind die FFmpeg-spezifischen Darstellungsoptionen nicht verfügbar.",formWidget);
    prepare_info->setWordWrap(true); form->addWidget(prepare_info);
    auto *apply=new QPushButton("Einstellungen für dieses Video übernehmen",formWidget);
    form->addWidget(apply); form->addStretch();
    scroll->setWidget(formWidget);
    splitter->addWidget(scenes); splitter->addWidget(media); splitter->addWidget(scroll);
    splitter->setStretchFactor(2,1);
    layout->addWidget(splitter,1);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,&dialog);
    window_layout->addWidget(buttons);
    auto *edit_status=new QLabel(video_page); edit_status->setWordWrap(true); layout->addWidget(edit_status);

    auto setCombo=[](QComboBox *cb,const QString &key) { int i=cb->findData(key); cb->setCurrentIndex(i>=0?i:0); };
    auto getCombo=[](QComboBox *cb) { return cb->currentData().toString(); };
    bool loading_rule=false;
    QString current_end_mode="hold";
    std::map<QWidget *,QString> original_tooltips;
    for (auto *control:video_controls) original_tooltips[control]=control->toolTip();
    original_tooltips[audio_mode]=audio_mode->toolTip();original_tooltips[prepare]=prepare->toolTip();
    auto update_rule_state=[&] {
        if (loading_rule) return;
        auto *item=media->currentItem();
        const bool have=item!=nullptr;
        const bool audio_only=have && item->data(Qt::UserRole+1).toBool();
        const bool ffmpeg=have && item->data(Qt::UserRole+2).toBool();
        const auto kind=!have?maobs::RuleSource::None:audio_only?maobs::RuleSource::LiveAudio:
            ffmpeg?maobs::RuleSource::FFmpeg:maobs::RuleSource::VLC;
        const int expected_ms=int(expected->value()*1000.0+0.5),lead_ms=int(before_end->value()*1000.0+0.5);
        const auto state=maobs::rule_availability(kind,enabled->isChecked(),ma_videokey(getCombo(start)),
            prepare->isChecked(),expected_ms,lead_ms);
        const bool video=have && !audio_only;
        const bool display_capable=ffmpeg && prepare->isChecked();
        const auto field_state=[&](QWidget *field,bool active,const QString &reason) {
            field->setEnabled(active);
            field->setToolTip(active?original_tooltips[field]:reason);
            if (auto *label=options->labelForField(field)) {label->setEnabled(active);label->setToolTip(active?QString():reason);}
        };
        const QString off="Zuerst die Steuerung dieser Quelle in dieser Szene aktivieren.";
        for (auto *control:video_controls) {
            control->setVisible(video);
            if (auto *label=options->labelForField(control)) label->setVisible(video);
            field_state(control,state.video,off);
        }
        field_state(audio_mode,state.edit,off);
        field_state(percent,state.percent,state.video?"Nur bei Videostart nach Fadefortschritt verfügbar.":off);
        field_state(delay,state.delay,state.video?"Nur beim zeitverzögerten Videostart verfügbar.":off);
        field_state(expected,state.expected,state.video?"Nur beim Start vor dem geplanten Fadeende verfügbar.":off);
        field_state(before_end,state.lead,state.expected?"Ohne geplante Fadedauer startet das Video beim tatsächlichen Fadeende.":
            state.video?"Nur beim Start vor dem geplanten Fadeende verfügbar.":off);
        const QString manual="Bei Nur manuell erfolgt kein automatischer Start durch Fade oder Goto.";
        field_state(same_goto,state.automatic_events,state.video?manual:off);
        field_state(goto_mode,state.automatic_events,state.video?manual:off);
        field_state(before,state.display,state.video?"Die Bilddarstellung wird durch die OBS-Quelleneinstellungen bestimmt. Für MediaLink-Bildoptionen ist eine vorbereitete FFmpeg-Quelle erforderlich.":off);
        prepare->setVisible(video);prepare_info->setVisible(video);
        field_state(prepare,state.prepare,ffmpeg?off:"Die automatische Vorbereitung ist nur für FFmpeg-Medienquellen verfügbar.");
        prepare_info->setText(ffmpeg?
            "Ohne Vorbereitung kann OBS eine Medienquelle bereits vor der Fade-Schwelle automatisch starten. Die ursprünglichen OBS-Optionen werden beim Abschalten der MA-Steuerung wiederhergestellt.":
            "Bei VLC werden die Bildoptionen der OBS-Quelle verwendet. Start, Stopp, Pause und Wiederholen bleiben verfügbar.");
        on_end->setVisible(video && display_capable);
        options->labelForField(on_end)->setVisible(video && display_capable);
        field_state(on_end,state.display,off);
        on_end_basic->setVisible(video && !display_capable);
        options->labelForField(on_end_basic)->setVisible(video && !display_capable);
        {QSignalBlocker a(on_end),b(on_end_basic);
            setCombo(on_end,current_end_mode);
            setCombo(on_end_basic,current_end_mode=="loop"?"loop":"obs");}
        display_info->setVisible(video && !display_capable);display_info->setEnabled(state.video);
        timing_note->setVisible(state.expected && !state.lead);
        timing_note->setText("Ohne geplante Fadedauer startet das Video beim tatsächlichen Fadeende.");
        timing_error->setVisible(state.invalid_timing);
        apply->setEnabled(have && !state.invalid_timing);
    };
    auto populate_rule=[&](const QString &scene,const QString &source,bool audio_only) {
        loading_rule=true;
        for (auto *control:video_controls) {
            control->setVisible(!audio_only);
            if (auto *label=options->labelForField(control)) label->setVisible(!audio_only);
        }
        prepare->setVisible(!audio_only);prepare_info->setVisible(!audio_only);
        enabled->setText(audio_only ? "Audio dieser Quelle in dieser Szene steuern" : "Medienquelle in dieser Szene steuern");
        apply->setText(audio_only ? "Audioeinstellungen für diese Quelle übernehmen" : "Einstellungen für dieses Video übernehmen");
        static_cast<QLabel *>(options->labelForField(audio_mode))->setText(audio_only ? "Audio" : "Video-Ton");
        audio_mode->setItemText(0,audio_only ? "OBS-Pegel (ohne MA-Fade)" : "OBS Mix / Ab Videostart (ohne MA-Fade)");
        MAVideoRule r=ma_video.get(scene,source);
        { QSignalBlocker blocker(enabled); enabled->setChecked(r.enabled); }
        setCombo(audio_mode,r.audio_mode); setCombo(start,r.start); percent->setValue(r.percent);
        delay->setValue(r.delay_ms/1000.0);
        expected->setValue(r.expected_fade_ms/1000.0);
        before_end->setValue(r.lead_before_end_ms/1000.0);
        setCombo(on_exit,r.on_exit); setCombo(reentry,r.reentry);
        current_end_mode=r.on_end;setCombo(on_end,r.on_end); setCombo(before,r.before); setCombo(on_abort,r.on_abort);
        setCombo(same_goto,r.same_goto); setCombo(shared,r.shared); setCombo(goto_mode,r.goto_mode);
        prepare->setChecked(r.prepare_source);
        loading_rule=false;update_rule_state();
    };
    QString previous_scene, previous_source;
    bool previous_audio_only=false;
    bool video_view_ready=false;
    auto selection=[](QListWidget *list) {
        return list->currentItem() ? list->currentItem()->data(Qt::UserRole).toString() : QString();
    };
    auto select_uuid=[](QListWidget *list,const QString &uuid) {
        int row=list->count() ? 0 : -1;
        for (int i=0;i<list->count();++i)
            if (list->item(i)->data(Qt::UserRole).toString()==uuid) {row=i;break;}
        list->setCurrentRow(row);
    };
    auto remember_view=[&] {
        if (!previous_scene.isEmpty()) {
            view.setValue("selection/scene",previous_scene);
            view.setValue("selection/media/"+previous_scene,previous_source);
            if (video_view_ready) {
                view.setValue("media_scroll/"+previous_scene,media->verticalScrollBar()->value());
                view.setValue("media_scroll_horizontal/"+previous_scene,media->horizontalScrollBar()->value());
            }
        }
        if (video_view_ready && !previous_scene.isEmpty() && !previous_source.isEmpty()) {
            const QString key="form_scroll/"+previous_scene+"/"+previous_source+"/";
            view.setValue(key+"vertical",scroll->verticalScrollBar()->value());
            view.setValue(key+"horizontal",scroll->horizontalScrollBar()->value());
        }
        if (video_view_ready) {
            view.setValue("scene_scroll",scenes->verticalScrollBar()->value());
            view.setValue("scene_scroll_horizontal",scenes->horizontalScrollBar()->value());
        }
    };
    auto restore_scroll=[&] {
        const QString scene=selection(scenes), source=selection(media);
        const QString key="form_scroll/"+scene+"/"+source+"/";
        const int vertical=view.value(key+"vertical",0).toInt();
        const int horizontal=view.value(key+"horizontal",0).toInt();
        const int media_y=view.value("media_scroll/"+scene,0).toInt();
        const int media_x=view.value("media_scroll_horizontal/"+scene,0).toInt();
        const int scene_y=view.value("scene_scroll",0).toInt();
        const int scene_x=view.value("scene_scroll_horizontal",0).toInt();
        // Restore after the tab/scroll area has a real viewport. UUID guards
        // prevent a queued restore from moving a newly selected source's view.
        QTimer::singleShot(0,&dialog,[=,&video_view_ready] {
            if (!video_page->isVisible() || selection(scenes)!=scene || selection(media)!=source) return;
            scroll->verticalScrollBar()->setValue(vertical);
            scroll->horizontalScrollBar()->setValue(horizontal);
            media->verticalScrollBar()->setValue(media_y);
            media->horizontalScrollBar()->setValue(media_x);
            scenes->verticalScrollBar()->setValue(scene_y);
            scenes->horizontalScrollBar()->setValue(scene_x);
            video_view_ready=true;
        });
    };
    auto populate_media=[&] {
        const QString scene=selection(scenes);
        QSignalBlocker blocker(media); media->clear();
        for (const auto &entry:ma_control_sources_in_scene(scene)) {
            const MAVideoRule rule=ma_video.get(scene,entry.uuid);
            auto *item=new QListWidgetItem(entry.name+(entry.audio_only ? " [Blackmagic · Audio]" : entry.ffmpeg ? "" : " [VLC]"),media);
            item->setIcon(rule.enabled ? ma_media_icon("enabled") : QIcon());
            item->setData(Qt::UserRole,entry.uuid);
            item->setData(Qt::UserRole+1,entry.audio_only);
            item->setData(Qt::UserRole+2,entry.ffmpeg);
        }
        select_uuid(media,view.value("selection/media/"+scene).toString());
        const QString source=selection(media);
        selected->setText(media->currentItem() ? media->currentItem()->text() : "Diese Szene hat keine Medien- oder Blackmagic-Quelle");
        formWidget->setEnabled(!source.isEmpty());
        if (!source.isEmpty()) populate_rule(scene,source,media->currentItem()->data(Qt::UserRole+1).toBool());
        else update_rule_state();
    };
    auto populate_scenes=[&] {
        QSignalBlocker blocker(scenes); scenes->clear();
        int index=1;
        for (const auto &scene:ma_video_scenes()) {
            auto *item=new QListWidgetItem(QString("OBS:%1 · %2").arg(index++).arg(scene.second),scenes);
            item->setData(Qt::UserRole,scene.first);
        }
        select_uuid(scenes,view.value("selection/scene").toString());
        populate_media();
    };
    auto save_previous=[&] {
        if (previous_scene.isEmpty() || previous_source.isEmpty()) return;
        // Save the old form under its explicit UUID pair before loading another selection.
        MAVideoRule r=ma_video.get(previous_scene,previous_source);
        r.enabled=enabled->isChecked(); r.audio_mode=getCombo(audio_mode);
        if (!previous_audio_only) {
            r.start=getCombo(start); r.percent=percent->value();
            r.delay_ms=int(delay->value()*1000.0+0.5);
            r.expected_fade_ms=int(expected->value()*1000.0+0.5);
            r.lead_before_end_ms=int(before_end->value()*1000.0+0.5);
            r.on_exit=getCombo(on_exit);
            r.reentry=getCombo(reentry); r.on_end=current_end_mode; r.before=getCombo(before);
            r.on_abort=getCombo(on_abort); r.same_goto=getCombo(same_goto);
            r.shared=getCombo(shared); r.goto_mode=getCombo(goto_mode);
            r.prepare_source=prepare->isChecked();
        }
        ma_video.set(previous_scene,previous_source,r);
    };
    auto set_selection=[&] {
        previous_scene=selection(scenes); previous_source=selection(media); video_view_ready=false;
        previous_audio_only=media->currentItem() && media->currentItem()->data(Qt::UserRole+1).toBool();
    };
    QObject::connect(enabled,&QCheckBox::toggled,&dialog,[&](bool active) {
        if (auto *item=media->currentItem())
            item->setIcon(active ? ma_media_icon("enabled") : QIcon());
        update_rule_state();
    });
    QObject::connect(start,&QComboBox::currentIndexChanged,&dialog,[&](int) {update_rule_state();});
    QObject::connect(expected,&QDoubleSpinBox::valueChanged,&dialog,[&](double) {update_rule_state();});
    QObject::connect(before_end,&QDoubleSpinBox::valueChanged,&dialog,[&](double) {update_rule_state();});
    QObject::connect(prepare,&QCheckBox::toggled,&dialog,[&](bool) {update_rule_state();});
    QObject::connect(on_end,&QComboBox::currentIndexChanged,&dialog,[&](int) {
        if (loading_rule) return;
        current_end_mode=getCombo(on_end);update_rule_state();
    });
    QObject::connect(on_end_basic,&QComboBox::currentIndexChanged,&dialog,[&](int) {
        if (loading_rule) return;
        current_end_mode=ma_utf(maobs::basic_end_mode(ma_videokey(current_end_mode),getCombo(on_end_basic)=="loop").c_str());
        update_rule_state();
    });
    QObject::connect(media,&QListWidget::currentRowChanged,&dialog,[&](int) {
        remember_view(); save_previous();
        const QString scene=selection(scenes), source=selection(media);
        formWidget->setEnabled(!source.isEmpty());
        if (!source.isEmpty()) {selected->setText(media->currentItem()->text()); populate_rule(scene,source,media->currentItem()->data(Qt::UserRole+1).toBool());}
        else update_rule_state();
        set_selection(); restore_scroll(); edit_status->clear();
    });
    QObject::connect(scenes,&QListWidget::currentRowChanged,&dialog,[&](int) {
        remember_view(); save_previous(); previous_scene.clear(); previous_source.clear();
        populate_media(); set_selection(); restore_scroll(); edit_status->clear();
    });
    QObject::connect(reload,&QPushButton::clicked,&dialog,[&] {
        remember_view(); save_previous(); previous_scene.clear(); previous_source.clear();
        populate_scenes(); set_selection(); restore_scroll();
    });
    QObject::connect(apply,&QPushButton::clicked,&dialog,[&] {
        save_previous(); remember_view();
        edit_status->setText("Regel übernommen.");
    });
    tabs->addTab(ma_create_diagnostic_panel(tabs),"Diagnose");
    auto *about=new QWidget(tabs);
    auto *about_layout=new QVBoxLayout(about);
    ma_add_about_identity(about,about_layout);
    auto *name=new QLabel("MA OBS MediaLink",about);
    name->setStyleSheet("font-size:22px; font-weight:bold;"); about_layout->addWidget(name);
    about_layout->addWidget(new QLabel("Version 0.46 · BETA",about));
    auto *summary=new QLabel("Szenen, Video und Audio aus grandMA3 steuern.",about);
    summary->setWordWrap(true); about_layout->addWidget(summary);
    about_layout->addWidget(new QLabel("Copyright © 2026 Oliver Cassel",about));
    ma_add_repository_button(about,about_layout);
    about_layout->addWidget(ma_create_dedication(about));
    auto *beta_note=new QLabel("Diese Beta-Version befindet sich in Entwicklung.",about);
    beta_note->setWordWrap(true); about_layout->addWidget(beta_note); about_layout->addStretch();
    tabs->addTab(about,"Über");
    buttons->button(QDialogButtonBox::Save)->setText("Speichern");
    buttons->button(QDialogButtonBox::Cancel)->setText("Abbrechen");
    const auto same_collection=[&] {
        return original_epoch==ma_current_collection_epoch() && !ma_video.suspended &&
            original_collection==ma_video_collection_name();
    };
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&] {
        if (!same_collection()) {dialog.reject(); return;}
        save_previous();
        for (const auto &scene:ma_video_scenes()) for (const auto &entry:ma_control_sources_in_scene(scene.first)) {
            const auto rule=ma_video.get(scene.first,entry.uuid);
            const auto kind=entry.audio_only?maobs::RuleSource::LiveAudio:entry.ffmpeg?maobs::RuleSource::FFmpeg:maobs::RuleSource::VLC;
            if (maobs::rule_availability(kind,rule.enabled,ma_videokey(rule.start),rule.prepare_source,
                rule.expected_fade_ms,rule.lead_before_end_ms).invalid_timing) {
                QMessageBox::warning(&dialog,"MA OBS MediaLink","Szene: "+scene.second+"\nQuelle: "+entry.name+
                    "\n\nDer Startabstand vor dem Fadeende darf nicht größer als die geplante Fadedauer sein.");
                return;
            }
        }
        if (!save_native()) return;
        ma_video.enabled=global->isChecked();
        if (!ma_video.save()) {
            QMessageBox::warning(&dialog,"MA OBS MediaLink",
                "Allgemeine Einstellungen wurden gespeichert; Video-/Audioregeln konnten nicht gespeichert werden. Bitte erneut versuchen.");
            return;
        }
        // Saving display/rules never resets runtime or restarts media sources.
        if (!ma_video.enabled && saved_enabled) ma_video.stop_pilot();
        dialog.accept();
    });
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    // Close safely if an external collection load happens during the modal UI.
    QTimer collection_guard(&dialog);
    QObject::connect(&collection_guard,&QTimer::timeout,&dialog,[&] {
        if (!same_collection()) dialog.reject();
    });
    collection_guard.start(150);
    populate_scenes(); set_selection();
    const auto catalog_signature=[&] {
        QStringList result;
        const auto all_scenes=ma_video_scenes();
        result.append(QString::number(all_scenes.size()));
        for (const auto &scene:all_scenes) {result.append(scene.first);result.append(scene.second);}
        const QString scene=selection(scenes); result.append(scene);
        for (const auto &media_entry:ma_control_sources_in_scene(scene)) {
            result.append(media_entry.uuid); result.append(media_entry.name);
            result.append(media_entry.audio_only ? "blackmagic" : media_entry.ffmpeg ? "ffmpeg" : "vlc");
        }
        return result;
    };
    QStringList last_catalog=catalog_signature();
    QString last_catalog_scene=selection(scenes);
    QTimer catalog_timer(&dialog);
    QObject::connect(&catalog_timer,&QTimer::timeout,&dialog,[&] {
        if (!same_collection() || !video_page->isVisible()) return;
        const auto current_catalog=catalog_signature();
        // A deliberate scene selection already populated the editor synchronously.
        if (selection(scenes)!=last_catalog_scene) {
            last_catalog_scene=selection(scenes); last_catalog=current_catalog; return;
        }
        if (current_catalog==last_catalog) return;
        // Keep unsaved form values and UUID selection while refreshing lists.
        // Draft rules do not reach the runtime until Save succeeds.
        remember_view(); save_previous(); previous_scene.clear(); previous_source.clear();
        populate_scenes(); set_selection(); restore_scroll();
        last_catalog=catalog_signature(); last_catalog_scene=selection(scenes);
    });
    catalog_timer.start(350);
    const QByteArray splitter_state=view.value("splitter").toByteArray();
    if (!splitter_state.isEmpty()) splitter->restoreState(splitter_state);
    const int tab=initial_tab>=0 ? initial_tab : view.value("tab",0).toInt();
    tabs->setCurrentIndex(std::clamp(tab,0,tabs->count()-1));
    QObject::connect(tabs,&QTabWidget::currentChanged,&dialog,[&](int index) {
        if (index==2) restore_scroll(); else remember_view();
    });
    const int scene_y=view.value("scene_scroll",0).toInt();
    QTimer::singleShot(0,&dialog,[=] {scenes->verticalScrollBar()->setValue(scene_y);});
    restore_scroll();
    const int outcome=dialog.exec();
    collection_guard.stop(); catalog_timer.stop();notice_timer->stop();
    remember_view();
    view.setValue("geometry",dialog.saveGeometry());
    if (video_view_ready) view.setValue("splitter",splitter->saveState());
    view.setValue("tab",tabs->currentIndex()); view.sync();
    if (view.status()!=QSettings::NoError)
        blog(LOG_WARNING,"MA OBS MediaLink: view preferences could not be saved");
    if (outcome!=QDialog::Accepted && same_collection()) {
        ma_video.rules=saved_rules; ma_video.enabled=saved_enabled;
    }
    ma_settings_window=nullptr;
}

#include "media_panel.h"
