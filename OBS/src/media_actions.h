// MA OBS MediaLink v0.46. OBS media commands, independent of the Qt display.
#pragma once
enum class MAMediaIntent { Toggle, Play, Pause, Restart, Stop };
enum class MAMediaCommand { None, Play, Pause, Restart, Stop };

static bool ma_media_can_pause(obs_media_state state) {
    return state==OBS_MEDIA_STATE_PLAYING || state==OBS_MEDIA_STATE_BUFFERING ||
        state==OBS_MEDIA_STATE_OPENING;
}
static bool ma_media_is_playing(obs_media_state state) {
    return state==OBS_MEDIA_STATE_PLAYING;
}
static MAMediaCommand ma_resolve_media_command(MAMediaIntent intent,obs_media_state state) {
    switch (intent) {
    case MAMediaIntent::Restart: return MAMediaCommand::Restart;
    case MAMediaIntent::Stop: return MAMediaCommand::Stop;
    case MAMediaIntent::Pause: return MAMediaCommand::Pause;
    case MAMediaIntent::Toggle:
        if (ma_media_can_pause(state)) return MAMediaCommand::Pause;
        // fall through to state-aware Play
        [[fallthrough]];
    case MAMediaIntent::Play:
        if (state==OBS_MEDIA_STATE_PAUSED) return MAMediaCommand::Play;
        if (ma_media_can_pause(state)) return MAMediaCommand::None;
        // OBS play_pause(false) does not open/restart stopped/ended FFmpeg media.
        return MAMediaCommand::Restart;
    }
    return MAMediaCommand::None;
}
static void ma_execute_media_command(obs_source_t *source,MAMediaCommand command) {
    switch (command) {
    case MAMediaCommand::Play: obs_source_media_play_pause(source,false); break;
    case MAMediaCommand::Pause: obs_source_media_play_pause(source,true); break;
    case MAMediaCommand::Restart: obs_source_media_restart(source); break;
    case MAMediaCommand::Stop: obs_source_media_stop(source); break;
    case MAMediaCommand::None: break;
    }
}
static bool ma_media_automatic_start_allowed(bool fired,bool manual_override,bool manual_rule) {
    return !fired && !manual_override && !manual_rule;
}
