#include <obs-module.h>
#include "cue_math.h"
#include "live_audio_policy.h"
#include "settings_policy.h"
#include <util/platform.h>
#include <graphics/graphics.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <utility>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <obs-frontend-api.h>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QString>
#include <QSignalBlocker>
#include <QByteArray>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QScrollBar>
#include <QStringList>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QClipboard>
#include <QApplication>
#include <QVariant>
#include <QTabWidget>
#include <QSettings>
#include <QPointer>
#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>
#include <QAbstractButton>
#include <QScreen>
#include <functional>
#include <memory>
#include <map>
#include <set>
#include <cmath>
#include <QListWidget>
#include <QSplitter>
#include <QScrollArea>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QDateTime>
#include <QFrame>
#include <QPalette>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-sacn-crossfade", "en-US")
MODULE_EXPORT const char *obs_module_description(void) { return "MA OBS MediaLink – Szenen, Video und Audio aus grandMA3 steuern (v0.45 BETA)"; }

namespace {
static bool ma_obs_exiting=false;
static bool ma_frontend_available=true;
static bool ma_media_control_active();
static QWidget *ma_create_video_panel(QWidget *parent);
static void ma_settings_dialog(int initial_tab=-1);
static void native_settings_dialog(void *);
static void ma_add_native_pages(QDialog *, QTabWidget *, std::function<bool()> &);
static QWidget *ma_create_diagnostic_panel(QWidget *);
static uint64_t ma_current_collection_epoch();
static void pilot_toggle(void *);
struct Receiver {
    std::atomic<bool> running{false};
    std::atomic<uint16_t> level{0};
    std::atomic<uint64_t> last_packet_ms{0};
    std::atomic<bool> socket_bound{false};
    std::atomic<bool> multicast_joined{false};
    std::atomic<int> last_socket_error{0};
    std::atomic<uint32_t> sender_ipv4{0}; // network byte order
    std::atomic<uint32_t> next_events{0};
    std::atomic<uint32_t> previous_events{0};
    struct CueFrame {
        uint32_t raw=0, endpoint=0;
        uint16_t scene=0, token=0;
        uint64_t received_ms=0;
        uint64_t frame_serial=0;
    };
    std::mutex frame_mutex;
    CueFrame cue_frame;
    bool cue_mode=false;
    std::thread worker;
    uint16_t universe = 10;
    uint16_t address = 1;
    std::string bind_ip = "0.0.0.0";
    std::string multicast_interface = "192.168.1.71";
    bool multicast = false;
    // All settings are copied into the worker before start: no thread reads
    // strings while OBS may replace them in update().
    struct Config {
        uint16_t universe, address;
        std::string bind_ip, interface_ip;
        bool multicast;
        bool cue_mode;
        bool operator==(const Config &o) const {
            return universe == o.universe && address == o.address &&
                   bind_ip == o.bind_ip && interface_ip == o.interface_ip &&
                   multicast == o.multicast && cue_mode == o.cue_mode;
        }
    };
    Config config() const { return {universe,address,bind_ip,multicast_interface,multicast,cue_mode}; }
    void stop() {
        running.store(false);
        if (worker.joinable()) worker.join();
        socket_bound = false;
        multicast_joined = false;
    }
    void start(const Config &cfg) {
        stop();
        universe = cfg.universe;
        address = cfg.address;
        bind_ip = cfg.bind_ip;
        multicast_interface = cfg.interface_ip;
        multicast = cfg.multicast;
        sender_ipv4 = 0;
        last_packet_ms = 0;
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            cue_frame = {};
        }
        last_socket_error = 0;
        running = true;
        worker = std::thread([this, cfg] { run(cfg); });
    }
    void run(const Config &cfg) {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) { last_socket_error = WSAGetLastError(); return; }
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) {
            last_socket_error = WSAGetLastError();
            WSACleanup(); return;
        }
        BOOL reuse = TRUE;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        DWORD timeout = 250;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(5568);
        // Multicast listeners bind to INADDR_ANY; select the physical NIC
        // exclusively via IP_ADD_MEMBERSHIP. Unicast may bind to a local IP.
        if (!cfg.multicast && cfg.bind_ip != "0.0.0.0") {
            if (inet_pton(AF_INET, cfg.bind_ip.c_str(), &addr.sin_addr) != 1) {
                blog(LOG_ERROR, "sACN: invalid unicast bind IPv4 '%s'", cfg.bind_ip.c_str());
                last_socket_error = WSAEINVAL;
                closesocket(s); WSACleanup(); return;
            }
        } else {
            addr.sin_addr.s_addr = INADDR_ANY;
        }
        if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            const int error = WSAGetLastError(); last_socket_error = error;
            blog(LOG_ERROR, "sACN: UDP bind failed: %d", error);
            closesocket(s); WSACleanup(); return;
        }
        socket_bound = true;
        if (cfg.multicast) {
            ip_mreq group{};
            const uint32_t multi = (239u << 24) | (255u << 16) |
                ((cfg.universe >> 8) << 8) | (cfg.universe & 255);
            group.imr_multiaddr.s_addr = htonl(multi);
            if (inet_pton(AF_INET, cfg.interface_ip.c_str(), &group.imr_interface) != 1) {
                blog(LOG_ERROR, "sACN: invalid multicast interface IPv4 '%s'", cfg.interface_ip.c_str());
                last_socket_error = WSAEINVAL;
                closesocket(s); socket_bound = false; WSACleanup(); return;
            }
            if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                           reinterpret_cast<const char*>(&group), sizeof(group)) != 0) {
                const int error = WSAGetLastError(); last_socket_error = error;
                blog(LOG_ERROR, "sACN: multicast join failed on %s: %d", cfg.interface_ip.c_str(), error);
                closesocket(s); socket_bound = false; WSACleanup(); return;
            }
            multicast_joined = true;
            blog(LOG_INFO, "MA Crossfade: sACN multicast U%u joined on %s",
                 unsigned(cfg.universe), cfg.interface_ip.c_str());
        } else {
            blog(LOG_INFO, "MA Crossfade: sACN unicast U%u listening on %s:5568",
                 unsigned(cfg.universe), cfg.bind_ip.c_str());
        }
        uint8_t packet[1500];
        bool next_high = false, previous_high = false;
        while (running.load()) {
            sockaddr_in sender{};
            int sender_len = sizeof(sender);
            const int n = recvfrom(s, reinterpret_cast<char*>(packet), sizeof(packet), 0,
                                   reinterpret_cast<sockaddr*>(&sender), &sender_len);
            if (n < 0) {
                const int error = WSAGetLastError();
                if (error != WSAETIMEDOUT && error != WSAEWOULDBLOCK && running.load()) {
                    last_socket_error = error;
                    blog(LOG_WARNING, "sACN: recvfrom failed: %d", error);
                }
                continue;
            }
            if (n < 130) continue; // start code + four DMX control channels
            static const uint8_t acn_id[12] = {0x41,0x53,0x43,0x2d,0x45,0x31,0x2e,0x31,0x37,0x00,0x00,0x00};
            if (std::memcmp(packet + 4, acn_id, 12) != 0) continue;
            if (packet[18] != 0 || packet[19] != 0 || packet[20] != 0 || packet[21] != 4) continue;
            if (packet[40] != 0 || packet[41] != 0 || packet[42] != 0 || packet[43] != 2) continue;
            if (packet[117] != 2 || packet[118] != 0xa1) continue;
            const uint16_t u = (uint16_t(packet[113]) << 8) | packet[114];
            if (u != cfg.universe || packet[125] != 0) continue;
            // Stream termination: do not treat the last packet as a valid
            // active sender, retain the last mix position.
            if (packet[112] & 0x40) continue;
            const uint16_t count = (uint16_t(packet[123]) << 8) | packet[124];
            const int required = cfg.cue_mode ? 10 : 4;
            if (count < cfg.address + required || n < 125 + int(count)) continue;
            const size_t offset = 126 + cfg.address - 1;
            if (int(offset + required) > n) continue;
            const uint64_t ts = GetTickCount64();
            sender_ipv4 = sender.sin_addr.s_addr;
            last_packet_ms = ts;
            if (cfg.cue_mode) {
                // All four metadata fields come from ONE validated E1.31 DMX frame.
                CueFrame v{};
                v.raw = (uint32_t(packet[offset]) << 16) |
                        (uint32_t(packet[offset + 1]) << 8) | packet[offset + 2];
                v.scene = (uint16_t(packet[offset + 3]) << 8) | packet[offset + 4];
                v.endpoint = (uint32_t(packet[offset + 5]) << 16) |
                             (uint32_t(packet[offset + 6]) << 8) | packet[offset + 7];
                v.token = (uint16_t(packet[offset + 8]) << 8) | packet[offset + 9];
                level = uint16_t(v.raw >> 8);
                v.received_ms = ts;
                {
                    std::lock_guard<std::mutex> lock(frame_mutex);
                    v.frame_serial = cue_frame.frame_serial + 1;
                    cue_frame = v;
                }
                continue;
            }
            const uint16_t raw = (uint16_t(packet[offset]) << 8) | packet[offset + 1];
            level = raw;
            const bool n_high = packet[offset + 2] >= 128;
            const bool p_high = packet[offset + 3] >= 128;
            if (n_high && !next_high) {
                next_events.fetch_add(1);
                blog(LOG_INFO, "MA Crossfade: received NEXT trigger (DMX %u)", unsigned(packet[offset + 2]));
            }
            if (p_high && !previous_high) {
                previous_events.fetch_add(1);
                blog(LOG_INFO, "MA Crossfade: received PREVIOUS trigger (DMX %u)", unsigned(packet[offset + 3]));
            }
            next_high = n_high;
            previous_high = p_high;
        }
        closesocket(s);
        socket_bound = false;
        multicast_joined = false;
        WSACleanup();
    }
};
static std::string ipv4_string(uint32_t ipv4_network_order) {
    if (!ipv4_network_order) return "(none)";
    in_addr addr{}; addr.s_addr = ipv4_network_order;
    char buffer[INET_ADDRSTRLEN]{};
    return inet_ntop(AF_INET, &addr, buffer, sizeof(buffer)) ? buffer : "(invalid)";
}

struct MALiveLayer {
    std::string uuid;
    std::string name;
    float weight = 0.f;
};
static void ma_add_layer(std::vector<MALiveLayer> &layers, const MALiveLayer &layer) {
    if (layer.weight < 0.0001f) return;
    for (auto &existing : layers) {
        if ((!layer.uuid.empty() && layer.uuid == existing.uuid) ||
            (layer.uuid.empty() && existing.uuid.empty() && layer.name == existing.name)) {
            existing.weight += layer.weight; return;
        }
    }
    layers.push_back(layer);
}
static void ma_normalize_layers(std::vector<MALiveLayer> &layers) {
    layers.erase(std::remove_if(layers.begin(), layers.end(),
        [](const MALiveLayer &l){return l.weight < 0.0005f;}), layers.end());
    // Cap live render cost if dozens of transitions are interrupted in a row.
    if (layers.size() > 12) {
        std::stable_sort(layers.begin(), layers.end(), [](const MALiveLayer &a, const MALiveLayer &b) {
            return a.weight > b.weight;
        });
        layers.resize(12);
    }
    float total=0; for (const auto &l:layers) total+=l.weight;
    if (total > 0.00001f) for (auto &l:layers) l.weight/=total;
}
struct Source {
    obs_source_t *owner = nullptr;
    std::string name_a, name_b;
    // Playlist entries are stored by OBS in source settings (UTF-8 names).
    // Phase 0.2.0 does not automatically change the A/B banks.
    std::vector<std::string> playlist;
    std::vector<std::pair<uint16_t, std::string>> cue_map;
    struct AutoScene { std::string uuid; std::string name; };
    std::vector<AutoScene> auto_scenes;
    bool auto_scene_mode = false; // Enabled only by the private Program Pilot.
    std::string cue_from_uuid, cue_to_uuid;
    std::mutex state_mutex;
    // current_index corresponds to the last fully visible playlist entry.
    size_t current_index = 0;
    size_t target_index = 0;
    int current_bank = 0;  // 0 = A, 1 = B
    bool pending = false;
    bool wrap_around = false;
    bool initialized = false;
    bool restored_from_disk = false;
    bool cue_mode = false;
    bool cue_initialized = false;
    bool cue_from_frozen = false; // legacy diagnostic only; snapshots no longer used.
    // At a retarget event, the preceding composite is represented as live
    // weighted OBS scenes, not a frozen texrender. The target is blended over
    // these weighted layers according to cue_math.progress.
    std::vector<MALiveLayer> live_base;
    std::map<std::string, obs_source_t *> active_children; // GUI-thread only; released before collection unload
    bool cue_first_packet = true;
    bool cue_last_valid = false;
    uint16_t cue_last_token = 0;
    uint16_t cue_last_scene = 0;
    uint32_t cue_last_endpoint = 0;
    uint32_t cue_last_raw = 0;
    uint64_t cue_last_serial = 0;
    uint64_t cue_last_event_time = 0;
    unsigned cue_events = 0;
    std::string cue_from_name, cue_to_name;
    std::string cue_warning;
    maobs::CueMath cue_math;
    uint32_t last_next_event = 0, last_previous_event = 0;
    Receiver receiver;
    bool network_initialized = false;
    Receiver::Config last_network_config{};
    gs_texrender_t *tex_a = nullptr, *tex_b = nullptr;
    gs_effect_t *effect = nullptr;
    uint32_t width = 1920, height = 1080;
};
// Save playback position independently of OBS scene-collection save timing.
// UUID keeps separate Crossfade source instances isolated.
static std::string state_path(Source *s) {
    const char *uuid = obs_source_get_uuid(s->owner);
    if (!uuid || !*uuid) return {};
    char *dir = obs_module_config_path("state");
    if (!dir) return {};
    os_mkdirs(dir);
    std::string result = std::string(dir) + "/" + uuid + ".json";
    bfree(dir);
    return result;
}
static uint64_t playlist_signature(const std::vector<std::string> &list) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const auto &name : list) {
        for (unsigned char ch : name) { hash ^= ch; hash *= UINT64_C(1099511628211); }
        hash ^= 0xff;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
static void persist_position(Source *s) {
    if (s->playlist.empty()) return;
    const std::string file = state_path(s);
    if (file.empty()) return;
    obs_data_t *state = obs_data_create();
    // Save as string: JSON numeric values cannot exactly represent all 64-bit hashes.
    const std::string signature = std::to_string(playlist_signature(s->playlist));
    obs_data_set_string(state, "playlist_signature", signature.c_str());
    obs_data_set_int(state, "position", int64_t(s->current_index));
    obs_data_set_int(state, "bank", int64_t(s->current_bank));
    const bool ok = obs_data_save_json_safe(state, file.c_str(), "tmp", "bak");
    if (!ok) blog(LOG_WARNING, "MA Crossfade: unable to persist playback position");
    obs_data_release(state);
}
static bool restore_position(Source *s) {
    const std::string file = state_path(s);
    if (file.empty()) return false;
    obs_data_t *state = obs_data_create_from_json_file(file.c_str());
    if (!state) return false;
    const std::string expected = std::to_string(playlist_signature(s->playlist));
    const char *stored = obs_data_get_string(state, "playlist_signature");
    const bool compatible = stored && expected == stored;
    if (compatible) {
        const int64_t index = obs_data_get_int(state, "position");
        const int64_t bank = obs_data_get_int(state, "bank");
        if (index >= 0 && uint64_t(index) < s->playlist.size() && (bank == 0 || bank == 1)) {
            s->current_index = size_t(index);
            s->current_bank = int(bank);
            s->target_index = s->current_index;
            s->pending = false;
            blog(LOG_INFO, "MA Crossfade: restored playlist position %zu (bank %c)",
                 s->current_index + 1, s->current_bank == 0 ? 'A' : 'B');
            obs_data_release(state);
            return true;
        }
    }
    obs_data_release(state);
    return false;
}
static const char *source_name(void*) { return "MA OBS MediaLink (BETA)"; }
// OBS source names are UTF-8. Supply names directly from OBS rather than
// asking users to type potentially misencoded strings into the properties.
static bool add_scene(void *param, obs_source_t *scene) {
    auto *list = static_cast<obs_property_t *>(param);
    const char *name = obs_source_get_name(scene);
    if (name && *name) obs_property_list_add_string(list, name, name);
    return true;
}
static obs_properties_t *properties(void *data) {
    auto *s = static_cast<Source *>(data);
    obs_properties_t *p = obs_properties_create();
    std::string status = "Status unavailable (source not initialized)";
    if (s) {
        size_t active = 0, total = 0, target = 0;
        bool pending = false, wrap = false;
        int bank = 0;
        std::string active_name, target_name, visible_a, visible_b;
        {
            std::lock_guard<std::mutex> lock(s->state_mutex);
            total = s->playlist.size();
            if (total) {
                active = s->current_index + 1;
                active_name = s->playlist[s->current_index];
                if (s->pending) {
                    target = s->target_index + 1;
                    target_name = s->playlist[s->target_index];
                }
            }
            pending = s->pending;
            bank = s->current_bank;
            wrap = s->wrap_around;
            visible_a = s->name_a;
            visible_b = s->name_b;
        }
        const uint16_t level = s->receiver.level.load();
        const uint64_t stamp = s->receiver.last_packet_ms.load();
        const uint64_t now = GetTickCount64();
        const bool fresh = stamp && now >= stamp && now - stamp < 2500;
        const char *network = !s->receiver.socket_bound.load() ? "Socket unavailable"
                            : fresh ? "Receiving" : "Signal lost / no recent DMX";
        char first[256];
        std::snprintf(first, sizeof(first),
            "sACN: %s | Universe %u | DMX %u (%.1f%%)\n"
            "Playlist: %zu / %zu | Bank %c | Wrap: %s",
            network, unsigned(s->receiver.universe), unsigned(level),
            double(level) * 100.0 / 65535.0,
            active, total, bank == 0 ? 'A' : 'B', wrap ? "on" : "off");
        status = first;
        status += "\nCurrent: " + (active_name.empty() ? std::string("(none)") : active_name);
        status += "\nPrepared: " + (pending ? std::to_string(target) + " - " + target_name : "(none)");
        status += "\nA: " + visible_a + " | B: " + visible_b;
        status += "\nMode: " + std::string(s->receiver.multicast ? "Multicast" : "Unicast");
        status += " | Sender: " + ipv4_string(s->receiver.sender_ipv4.load());
        status += " | Socket error: " + std::to_string(s->receiver.last_socket_error.load());
        if (s->cue_mode) {
            std::lock_guard<std::mutex> lock(s->state_mutex);
            status += "\nCUE MODE: event=" + std::to_string(s->cue_events)
                + " scene=" + std::to_string(s->cue_last_scene)
                + " token=" + std::to_string(s->cue_last_token)
                + " progress=" + std::to_string(int(s->cue_math.progress*100.0f)) + "%";
            status += "\nFrom: " + s->cue_from_name + " | To: " + s->cue_to_name;
            if (!s->cue_warning.empty()) status += "\nWarning: " + s->cue_warning;
        }
        status += "\nSnapshot when properties were opened; reopen to refresh.";
    }
    obs_property_t *info = obs_properties_add_text(p, "live_status", status.c_str(), OBS_TEXT_INFO);
    obs_property_text_set_info_word_wrap(info, true);
    obs_property_t *mode_switch = obs_properties_add_list(p, "control_type", "Controller mode",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(mode_switch, "Playlist: NEXT / PREVIOUS (U10)", "playlist");
    obs_property_list_add_string(mode_switch, "Cue: 24-Bit-Fade + Zielwert (MA OBS MediaLink · BETA)", "cue");
    obs_properties_add_text(p, "cue_help", "Cue v0.14: DMX 1-3 Fade (24-bit), 4-5 Scene (16-bit), 6-8 Endpoint (24-bit), 9-10 Cue ID (16-bit). ChannelSet position must match the SET value in the MA Cue Note. First event initializes video; interrupted fades retain live scene layers (no frame snapshot).", OBS_TEXT_INFO);
    // Playlist owns scene selection. Legacy scene_a/scene_b settings are kept
    // internally for backwards compatibility with empty playlists.
    obs_properties_add_editable_list(
        p, "playlist", "Playlist (for NEXT/PREV mode)",
        OBS_EDITABLE_LIST_TYPE_STRINGS, nullptr, nullptr);
    obs_properties_add_editable_list(
        p, "cue_map", "Cue scene map: one entry per ID, e.g. 37=Begruessung",
        OBS_EDITABLE_LIST_TYPE_STRINGS, nullptr, nullptr);
    obs_properties_add_bool(p, "wrap_around", "Wrap around playlist (last to first / first to last)");
    obs_properties_add_text(p, "control_help", "DMX 3 = Next / DMX 4 = Previous (trigger >=128)", OBS_TEXT_INFO);
    obs_properties_add_int(p, "universe", "sACN Universe", 1, 63999, 1);
    obs_properties_add_int(p, "address", "DMX start address (coarse)", 1, 511, 1);
    obs_property_t *mode = obs_properties_add_list(p, "network_mode", "sACN receive mode",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(mode, "Unicast", "unicast");
    obs_property_list_add_string(mode, "Multicast", "multicast");
    obs_properties_add_text(p, "bind_ip", "Unicast bind IPv4 (0.0.0.0 = all)", OBS_TEXT_DEFAULT);
    obs_properties_add_text(p, "multicast_interface", "Multicast network card IPv4 (e.g. 192.168.1.71)", OBS_TEXT_DEFAULT);
    obs_properties_add_int(p, "width", "Output width", 16, 8192, 1);
    obs_properties_add_int(p, "height", "Output height", 16, 8192, 1);
    return p;
}
static void defaults(obs_data_t *d) {
    obs_data_set_default_bool(d, "wrap_around", false);
    obs_data_set_default_string(d, "control_type", "playlist");
    obs_data_set_default_int(d, "universe", 10);
    obs_data_set_default_int(d, "address", 1);
    obs_data_set_default_int(d, "width", 1920);
    obs_data_set_default_int(d, "height", 1080);
    obs_data_set_default_string(d, "bind_ip", "0.0.0.0");
    obs_data_set_default_string(d, "network_mode", "unicast");
    obs_data_set_default_string(d, "multicast_interface", "192.168.1.71");
}
// All live source pointers are protected by this registry mutex.
static std::mutex registry_mutex;
static std::vector<Source *> source_registry;

static void update(void *data, obs_data_t *d) {
    auto *s = static_cast<Source*>(data);
    const std::string configured_a = obs_data_get_string(d, "scene_a");
    const std::string configured_b = obs_data_get_string(d, "scene_b");
    // Read the playlist values as UTF-8; never keep pointers owned by obs_data.
    std::vector<std::string> entries;
    obs_data_array_t *array = obs_data_get_array(d, "playlist");
    if (array) {
        const size_t count = obs_data_array_count(array);
        entries.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            obs_data_t *item = obs_data_array_item(array, i);
            if (!item) continue;
            const char *value = obs_data_get_string(item, "value");
            if (value && *value) {
                // Preserve even duplicate entries: useful when a scene repeats.
                entries.emplace_back(value);
            }
            obs_data_release(item);
        }
        obs_data_array_release(array);
    }
    std::vector<std::pair<uint16_t, std::string>> cue_entries;
    obs_data_array_t *map_array = obs_data_get_array(d, "cue_map");
    if (map_array) {
        const size_t count = obs_data_array_count(map_array);
        for (size_t i=0;i<count;++i) {
            obs_data_t *item = obs_data_array_item(map_array, i);
            if (!item) continue;
            std::string value = obs_data_get_string(item, "value");
            obs_data_release(item);
            const size_t equals = value.find('=');
            if (equals == std::string::npos) continue;
            const std::string idtext = value.substr(0, equals);
            const auto digits = idtext.find_first_not_of(" \t0123456789");
            if (digits != std::string::npos || idtext.find_first_of("0123456789") == std::string::npos) continue;
            const unsigned long id = std::strtoul(idtext.c_str(), nullptr, 10);
            std::string name = value.substr(equals + 1);
            const auto beg = name.find_first_not_of(" \t");
            if (beg == std::string::npos) continue;
            name.erase(0,beg);
            const auto end = name.find_last_not_of(" \t");
            if (end != std::string::npos) name.erase(end+1);
            if (id>=1 && id<=65535 && !name.empty())
                cue_entries.emplace_back(uint16_t(id),name);
        }
        obs_data_array_release(map_array);
    }
    {
        std::lock_guard<std::mutex> lock(s->state_mutex);
        const bool map_changed = cue_entries != s->cue_map;
        s->cue_map = std::move(cue_entries);
        if (map_changed) { s->cue_initialized = false; s->cue_last_valid = false; }
        const bool new_cue_mode = std::strcmp(obs_data_get_string(d, "control_type"), "cue") == 0;
        const bool mode_changed = new_cue_mode != s->cue_mode;
        if (mode_changed) {
            s->cue_mode = new_cue_mode;
            s->cue_initialized = false;
            s->cue_first_packet = true;
            s->cue_from_frozen = false;
            s->cue_last_serial = 0;
            s->cue_last_token = 0;
            s->cue_last_scene = 0;
            s->cue_last_endpoint = 0;
            s->cue_last_valid = false;
            s->cue_events = 0;
            s->cue_warning.clear();
        }
        const bool changed = entries != s->playlist;
        if (new_cue_mode && changed) {
            s->cue_initialized = false;
            s->cue_first_packet = true;
        }
        s->wrap_around = obs_data_get_bool(d, "wrap_around");
        if (changed) {
            // Keep the logical current scene when the playlist is edited.
            // This also handles inserting items before the current position.
            const std::string old_current =
                s->current_index < s->playlist.size() ? s->playlist[s->current_index] : std::string{};
            const std::string old_target =
                s->pending && s->target_index < s->playlist.size()
                    ? s->playlist[s->target_index] : std::string{};
            const size_t old_index = s->current_index;
            const bool was_pending = s->pending;
            s->playlist = std::move(entries);
            if (s->playlist.empty()) {
                s->current_index = 0;
                s->target_index = 0;
                s->pending = false;
            } else if (!s->initialized) {
                s->current_index = 0;
                s->target_index = 0;
                s->pending = false;
                s->restored_from_disk = restore_position(s);
            } else {
                // Duplicate names are permitted: prefer the old index if it
                // still refers to the same entry; otherwise choose the nearest
                // matching occurrence, maintaining the selected logical scene.
                auto locate = [&](const std::string &name, size_t old_pos) -> size_t {
                    if (old_pos < s->playlist.size() && s->playlist[old_pos] == name) return old_pos;
                    size_t result = s->playlist.size();
                    size_t best_distance = SIZE_MAX;
                    for (size_t i = 0; i < s->playlist.size(); ++i) {
                        if (s->playlist[i] != name) continue;
                        const size_t distance = i > old_pos ? i - old_pos : old_pos - i;
                        if (distance < best_distance) { best_distance = distance; result = i; }
                    }
                    return result;
                };
                const size_t found = locate(old_current, old_index);
                s->current_index = found < s->playlist.size()
                    ? found : std::min(old_index, s->playlist.size() - 1);
                if (was_pending) {
                    const size_t target = locate(old_target, s->target_index);
                    s->pending = target < s->playlist.size() && target != s->current_index;
                    s->target_index = s->pending ? target : s->current_index;
                } else {
                    s->target_index = s->current_index;
                }
                persist_position(s);
                blog(LOG_INFO, "MA Crossfade: playlist edited, retaining position %zu", s->current_index + 1);
            }
        }
        s->initialized = true;
        if (!s->playlist.empty() && s->pending) {
            // Keep the visible bank unchanged; refresh only the destination.
            if (s->current_bank == 0) s->name_b = s->playlist[s->target_index];
            else s->name_a = s->playlist[s->target_index];
        }
        if (!s->playlist.empty()) {
            if (!s->pending) {
                const std::string &visible = s->playlist[s->current_index];
                // Keep both banks on the current scene until a NEXT/PREV is
                // armed. This prevents black frames after restarting OBS
                // while the MA is parked at the opposite DMX endpoint.
                s->name_a = visible;
                s->name_b = visible;
            }
        } else {
            s->name_a = configured_a;
            s->name_b = configured_b;
        }
        blog(LOG_INFO, "MA Crossfade: playlist entries=%zu, active=%zu, bank=%c, pending=%d",
             s->playlist.size(), s->playlist.empty() ? size_t(0) : s->current_index + 1,
             s->current_bank == 0 ? 'A' : 'B', int(s->pending));
    }
    s->width = uint32_t(obs_data_get_int(d, "width"));
    s->height = uint32_t(obs_data_get_int(d, "height"));
    Receiver::Config network{};
    network.universe = uint16_t(obs_data_get_int(d, "universe"));
    network.address = uint16_t(obs_data_get_int(d, "address"));
    network.bind_ip = obs_data_get_string(d, "bind_ip");
    network.interface_ip = obs_data_get_string(d, "multicast_interface");
    network.multicast = std::strcmp(obs_data_get_string(d, "network_mode"), "multicast") == 0;
    network.cue_mode = s->cue_mode;
    if (!s->network_initialized || !(network == s->last_network_config)) {
        s->last_network_config = network;
        s->network_initialized = true;
        s->receiver.start(network);
    }
}
static void *create(obs_data_t *d, obs_source_t *owner) {
    auto *s = new Source;
    s->owner = owner;
    obs_enter_graphics();
    s->tex_a = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    s->tex_b = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    char *path = obs_module_file("crossfade.effect");
    if (path) { s->effect = gs_effect_create_from_file(path, nullptr); bfree(path); }
    obs_leave_graphics();
    update(s, d);
    {
        std::lock_guard<std::mutex> lock(registry_mutex);
        source_registry.push_back(s);
    }
    return s;
}
static void destroy(void *data) {
    auto *s = static_cast<Source*>(data);
    {
        std::lock_guard<std::mutex> lock(registry_mutex);
        source_registry.erase(std::remove(source_registry.begin(), source_registry.end(), s), source_registry.end());
    }
    s->receiver.stop();
    obs_enter_graphics();
    if (s->tex_a) gs_texrender_destroy(s->tex_a);
    if (s->tex_b) gs_texrender_destroy(s->tex_b);
    if (s->effect) gs_effect_destroy(s->effect);
    obs_leave_graphics();
    delete s;
}
static uint32_t get_width(void *data) { return static_cast<Source*>(data)->width; }
static uint32_t get_height(void *data) { return static_cast<Source*>(data)->height; }
static void capture(gs_texrender_t *target, const std::string &name, uint32_t w, uint32_t h,
                    obs_source_t *owner, const std::string &uuid = {}) {
    if (!target || (uuid.empty() && name.empty())) return;
    obs_source_t *scene = !uuid.empty() ? obs_get_source_by_uuid(uuid.c_str()) : obs_get_source_by_name(name.c_str());
    if (!scene) return;
    // Avoid rendering the source recursively when the configured scene contains it.
    if (scene == owner) { obs_source_release(scene); return; }
    gs_texrender_reset(target);
    if (gs_texrender_begin(target, w, h)) {
        vec4 clear{};
        gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
        gs_ortho(0.0f, float(w), 0.0f, float(h), -100.0f, 100.0f);
        gs_matrix_push();
        const uint32_t sw = obs_source_get_width(scene), sh = obs_source_get_height(scene);
        if (sw && sh) gs_matrix_scale3f(float(w)/sw, float(h)/sh, 1.0f);
        obs_source_video_render(scene);
        gs_matrix_pop();
        gs_texrender_end(target);
    }
    obs_source_release(scene);
}
static void draw(gs_texture_t *texture, gs_effect_t *effect, float opacity, uint32_t w, uint32_t h) {
    if (!texture || !effect || opacity <= 0) return;
    gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
    gs_eparam_t *alpha = gs_effect_get_param_by_name(effect, "opacity");
    gs_effect_set_texture(image, texture);
    gs_effect_set_float(alpha, opacity);
    while (gs_effect_loop(effect, "Draw")) gs_draw_sprite(texture, 0, w, h);
}
static void render_cue(Source *s) {
    Receiver::CueFrame rx{};
    {
        std::lock_guard<std::mutex> lock(s->receiver.frame_mutex);
        rx = s->receiver.cue_frame;
    }
    // Missing/stale E1.31 means HOLD LAST FRAME, not a surprise black/scene switch.
    const uint64_t now = GetTickCount64();
    const bool fresh = rx.received_ms && now >= rx.received_ms && (now-rx.received_ms <= 2500);
    // Keep rendering the last selected picture while DMX is absent.
    // Returning here would yield a transparent frame in OBS (unwanted black).
    bool event = false;

    std::string next_name;
    std::string next_uuid;
    {
        std::lock_guard<std::mutex> lock(s->state_mutex);
        if (fresh && rx.token && rx.frame_serial != s->cue_last_serial) {
            event = !s->cue_last_valid || rx.token != s->cue_last_token ||
                    rx.scene != s->cue_last_scene || rx.endpoint != s->cue_last_endpoint;
            s->cue_last_serial = rx.frame_serial;
        }
        if (event) {
            if (s->auto_scene_mode) {
                if (rx.scene >= 1 && rx.scene <= s->auto_scenes.size()) {
                    const auto &entry = s->auto_scenes[rx.scene - 1];
                    next_uuid = entry.uuid;
                    next_name = entry.name;
                }
            } else if (!s->cue_map.empty()) {
                for (const auto &entry: s->cue_map) {
                    if (entry.first == rx.scene) { next_name = entry.second; break; }
                }
            } else if (rx.scene >= 1 && rx.scene <= s->playlist.size()) {
                // Backwards-compatible default: ID = 1-based playlist entry.
                next_name = s->playlist[rx.scene - 1];
            }
            if (next_name.empty()) {
                s->cue_warning = "Scene ID has no mapping; holding previous picture";
                blog(LOG_WARNING, "MA Cue: scene ID %u has no mapping", unsigned(rx.scene));
                event = false;
            }
            // Remember the invalid ID too, to avoid warning on every DMX frame.
            s->cue_last_valid = true;
            s->cue_last_token = rx.token;
            s->cue_last_scene = rx.scene;
            s->cue_last_endpoint = rx.endpoint;
        }
    }

    if (event && !next_name.empty()) {
        obs_source_t *check = !next_uuid.empty()
            ? obs_get_source_by_uuid(next_uuid.c_str()) : obs_get_source_by_name(next_name.c_str());
        if (!check || check == s->owner) {
            blog(LOG_WARNING, "MA Cue: OBS source '%s' missing or recursively selected; holding picture", next_name.c_str());
            if (check) obs_source_release(check);
            event = false;
        } else {
            obs_source_release(check);
        }
    }
    std::vector<MALiveLayer> frame_layers;
    {
        std::lock_guard<std::mutex> lock(s->state_mutex);
        if (event) {
            ++s->cue_events;
            if (!s->cue_initialized) {
                // Joining mid-show cannot safely reconstruct the previous picture.
                s->cue_from_name = next_name;
                s->cue_to_name = next_name;
                s->cue_from_uuid = next_uuid;
                s->cue_to_uuid = next_uuid;
                s->cue_math.begin(rx.endpoint,rx.endpoint);
                s->cue_initialized = true;
                s->cue_from_frozen = false;
                s->live_base = {{next_uuid, next_name, 1.0f}};
                blog(LOG_INFO,"MA Cue: initialized scene %u (%s), token=%u",
                    unsigned(rx.scene), next_name.c_str(), unsigned(rx.token));
            } else {
                // Retarget without freezing the visible picture. Capture the
                // CURRENT live blend as the new base of this transition.
                // Every outgoing video keeps changing in Program while fading.
                float old_p = std::clamp(s->cue_math.progress, 0.f, 1.f);
                for (auto &layer : s->live_base) layer.weight *= (1.f - old_p);
                ma_add_layer(s->live_base,
                    {s->cue_to_uuid, s->cue_to_name, old_p});
                ma_normalize_layers(s->live_base);
                if (s->live_base.empty())
                    s->live_base.push_back({s->cue_to_uuid, s->cue_to_name, 1.0f});
                s->cue_from_frozen = false;
                s->cue_from_name = s->cue_to_name;
                s->cue_from_uuid = s->cue_to_uuid;
                s->cue_to_name = next_name;
                s->cue_to_uuid = next_uuid;
                // Scene ID/target/token are atomic within U12 frame, but Lua
                // can dispatch after the physical DMX Fade already started.
                const uint32_t from_raw = s->cue_math.reached
                    ? s->cue_math.target : s->cue_last_raw;
                s->cue_math.begin(from_raw, rx.endpoint);
                if (s->cue_math.small_range) {
                    s->cue_warning = "No DMX movement: end value equals start; change cue fade endpoint";
                    blog(LOG_WARNING, "MA Cue: event %u has zero/small DMX range (raw=%u to=%u)",
                         s->cue_events, unsigned(from_raw), unsigned(rx.endpoint));
                } else {
                    s->cue_warning.clear();
                }
                blog(LOG_INFO,"MA Cue: event %u scene=%u token=%u start=%u end=%u live_layers=%zu",
                    s->cue_events, unsigned(rx.scene),unsigned(rx.token),
                    unsigned(from_raw),unsigned(rx.endpoint),s->live_base.size());
            }
        }
        if (!s->cue_initialized) return;
        s->cue_math.update(rx.raw);
        s->cue_last_raw = rx.raw;
        const float progress = std::clamp(s->cue_math.progress,0.f,1.f);
        frame_layers.reserve(s->live_base.size()+1);
        for (const auto &base : s->live_base)
            ma_add_layer(frame_layers,{base.uuid,base.name,base.weight*(1.f-progress)});
        ma_add_layer(frame_layers,{s->cue_to_uuid,s->cue_to_name,progress});
        ma_normalize_layers(frame_layers);
    } // IMPORTANT: no graphics/OBS drawing while holding state_mutex!
    gs_blend_state_push();
    gs_enable_blending(true);
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
    float cumulative=0.f;
    for (const auto &layer:frame_layers) {
        // Every outgoing video is recaptured live, even after retargeting.
        capture(s->tex_a,layer.name,s->width,s->height,s->owner,layer.uuid);
        cumulative+=layer.weight;
        float alpha=cumulative>0.00001f ? layer.weight/cumulative : 1.f;
        draw(gs_texrender_get_texture(s->tex_a),s->effect,alpha,s->width,s->height);
    }
    gs_blend_state_pop();
}

static void render(void *data, gs_effect_t*) {
    auto *s = static_cast<Source*>(data);
    if (!s->effect) return;
    // A scene can contain this source indirectly. Never re-enter our own
    // rendering callback: doing so could overflow the stack and crash OBS.
    static thread_local std::vector<const Source *> render_stack;
    if (std::find(render_stack.begin(), render_stack.end(), s) != render_stack.end()) {
        return;
    }
    render_stack.push_back(s);
    struct PopStack {
        std::vector<const Source *> &stack;
        ~PopStack() { stack.pop_back(); }
    } guard{render_stack};
    if (s->cue_mode) {
        render_cue(s);
        return;
    }
    std::string a, b;
    {
        std::lock_guard<std::mutex> lock(s->state_mutex);
        const uint16_t level = s->receiver.level.load();
        const uint32_t next = s->receiver.next_events.load();
        const uint32_t previous = s->receiver.previous_events.load();
        const bool next_pressed = next != s->last_next_event;
        const bool previous_pressed = previous != s->last_previous_event;
        s->last_next_event = next;
        s->last_previous_event = previous;
        // Ignore commands while fading, or while an earlier command is pending.
        // Playlist mode retains the original 16-bit endpoint tolerance.
        const bool at_a = level <= 2;
        const bool at_b = level >= 65533;
        if (next_pressed || previous_pressed) {
            blog(LOG_INFO, "MA Crossfade: control next=%d previous=%d playlist=%zu mix=%u pending=%d",
                 int(next_pressed), int(previous_pressed), s->playlist.size(), unsigned(level), int(s->pending));
        }
        if (!s->playlist.empty()) {
            if (s->pending && ((s->current_bank == 0 && at_b) ||
                               (s->current_bank == 1 && at_a))) {
                s->current_index = s->target_index;
                s->current_bank = 1 - s->current_bank;
                s->pending = false;
                // Both banks currently show either target or old content;
                // keep target on both sides after committing to avoid a flash.
                s->name_a = s->playlist[s->current_index];
                s->name_b = s->playlist[s->current_index];
                persist_position(s);
                blog(LOG_INFO, "MA Crossfade: committed playlist position %zu", s->current_index + 1);
            }
            if (!s->pending && (at_a || at_b)) {
                // On restart, stored bank may differ from MA DMX endpoint.
                // Both banks show the restored scene until a new command.
                s->current_bank = at_b ? 1 : 0;
            }
            const bool at_current = s->current_bank == 0 ? at_a : at_b;
            if (at_current && !s->pending && (next_pressed != previous_pressed)) {
                if (next_pressed && (s->current_index + 1 < s->playlist.size() || s->wrap_around)) {
                    s->target_index = (s->current_index + 1) % s->playlist.size();
                    s->pending = s->target_index != s->current_index;
                } else if (previous_pressed && (s->current_index > 0 || s->wrap_around)) {
                    s->target_index = (s->current_index + s->playlist.size() - 1) % s->playlist.size();
                    s->pending = s->target_index != s->current_index;
                }
                if (s->pending) {
                    if (s->current_bank == 0) s->name_b = s->playlist[s->target_index];
                    else s->name_a = s->playlist[s->target_index];
                    blog(LOG_INFO, "MA Crossfade: armed playlist position %zu", s->target_index + 1);
                }
            }
        }
        a = s->name_a;
        b = s->name_b;
    }
    capture(s->tex_a, a, s->width, s->height, s->owner);
    capture(s->tex_b, b, s->width, s->height, s->owner);
    const float mix = float(s->receiver.level.load()) / 65535.0f;
    gs_blend_state_push();
    gs_enable_blending(true);
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
    draw(gs_texrender_get_texture(s->tex_a), s->effect, 1.0f, s->width, s->height);
    draw(gs_texrender_get_texture(s->tex_b), s->effect, mix, s->width, s->height);
    gs_blend_state_pop();
}
// OBS composite audio: render the same live scene layers used by the video
// renderer. Keep all source lookups and references inside the callback;
// no stale scene pointer survives a scene-collection switch.
static std::vector<MALiveLayer> ma_live_frame_layers_locked(Source *s) {
    std::vector<MALiveLayer> result;
    if (!s->cue_initialized) return result;
    float p=std::clamp(s->cue_math.progress,0.f,1.f);
    result.reserve(s->live_base.size()+1);
    for (const auto &layer:s->live_base)
        ma_add_layer(result,{layer.uuid,layer.name,layer.weight*(1.f-p)});
    ma_add_layer(result,{s->cue_to_uuid,s->cue_to_name,p});
    ma_normalize_layers(result);
    return result;
}
static std::vector<MALiveLayer> ma_live_frame_layers(Source *s) {
    std::lock_guard<std::mutex> lock(s->state_mutex);
    return ma_live_frame_layers_locked(s);
}
static void ma_enum_active_sources(void *data, obs_source_enum_proc_t callback, void *param) {
    auto *s=static_cast<Source*>(data);
    auto layers=ma_live_frame_layers(s);
    for (const auto &layer:layers) {
        obs_source_t *child=!layer.uuid.empty()? obs_get_source_by_uuid(layer.uuid.c_str()):
            obs_get_source_by_name(layer.name.c_str());
        if (!child) continue;
        if (child!=s->owner) callback(s->owner,child,param);
        obs_source_release(child);
    }
}
#include "audio_sources.h"
static bool ma_audio_render(void *data, uint64_t *ts_out,
    struct obs_source_audio_mix *audio_output, uint32_t mixers,
    size_t channels, size_t sample_rate) {
    auto *s=static_cast<Source*>(data);
    auto layers=ma_live_frame_layers(s);
    std::set<std::string> managed;
    {
        std::lock_guard<std::mutex> lock(ma_audio_policy_mutex);
        managed=ma_audio_managed;
    }
    MAAudioLeaves leaves;
    for (const auto &layer:layers) {
        obs_source_t *child=!layer.uuid.empty()?obs_get_source_by_uuid(layer.uuid.c_str()):
            obs_get_source_by_name(layer.name.c_str());
        if (!child) continue;
        MAAudioLeaves local;
        std::set<obs_source_t *> path;
        MAAudioWalk walk{local,managed,path,s->owner,layer.weight,1.f,mixers};
        ma_collect_audio(child,walk);
        obs_source_release(child);
        for (const auto &kv:local) {
            const auto &from=kv.second;
            if (!from.source) continue;
            auto &to=leaves[kv.first];
            if (!to.source) to.source=obs_source_get_ref(from.source);
            const char *uuid=obs_source_get_uuid(from.source);
            const bool individual=uuid && managed.count(uuid);
            for (size_t m=0;m<MAX_AUDIO_MIXES;++m)
                to.weights[m]=individual ? std::max(to.weights[m],from.weights[m]) :
                    std::min(1.f,to.weights[m]+from.weights[m]);
        }
        ma_release_audio_leaves(local);
    }
    struct Part {MAAudioLeaf *leaf; uint64_t ts;};
    std::vector<Part> parts;
    uint64_t first_ts=0;
    for (auto &kv:leaves) {
        auto &leaf=kv.second;
        if (!leaf.source || obs_source_audio_pending(leaf.source)) continue;
        const uint64_t ts=obs_source_get_audio_timestamp(leaf.source);
        if (!ts) continue;
        if (!first_ts || ts<first_ts) first_ts=ts;
        parts.push_back({&leaf,ts});
    }
    if (!first_ts) {ma_release_audio_leaves(leaves);return false;}
    const size_t n=std::min(channels,size_t(MAX_AUDIO_CHANNELS));
    for (const auto &part:parts) {
        if (part.ts<first_ts) continue;
        // Audio timestamps are in ns; samples in the current 1024-frame block.
        const uint64_t ns=part.ts-first_ts;
        const size_t start=size_t((static_cast<long double>(ns)*sample_rate)/1000000000.0L);
        if (start>=AUDIO_OUTPUT_FRAMES) continue;
        const size_t count=AUDIO_OUTPUT_FRAMES-start;
        struct obs_source_audio_mix src{};
        obs_source_get_audio_mix(part.leaf->source,&src);
        for (size_t mix=0;mix<MAX_AUDIO_MIXES;++mix) {
            if (!(mixers & (1u<<mix))) continue;
            for (size_t ch=0;ch<n;++ch) {
                float *dst=audio_output->output[mix].data[ch];
                float *in=src.output[mix].data[ch];
                if (!dst || !in) continue;
                for (size_t j=0;j<count;++j)
                    dst[start+j]+=in[j]*part.leaf->weights[mix];
            }
        }
    }
    ma_release_audio_leaves(leaves);
    *ts_out=first_ts;
    return true;
}

}
namespace {
static std::atomic<bool> pilot_ui_active{false};
static std::atomic<bool> pilot_ui_output_owned{false};
#include "ui_support.h"
// Display preference only; independent of source/receiver/media settings.
static bool ma_read_monitor_preference(const char *key,bool fallback) {
    char *path=obs_module_config_path("monitor_ui.json");
    if (!path) return fallback;
    obs_data_t *data=obs_data_create_from_json_file_safe(path,"bak");
    bfree(path);
    if (!data) return fallback;
    const bool value=obs_data_has_user_value(data,key)?obs_data_get_bool(data,key):fallback;
    obs_data_release(data);
    return value;
}
static void ma_write_monitor_preference(const char *key,bool value) {
    char *path=obs_module_config_path("monitor_ui.json");
    if (!path) return;
    obs_data_t *data=obs_data_create_from_json_file_safe(path,"bak");
    if (!data) data=obs_data_create();
    obs_data_set_bool(data,key,value);
    if (!obs_data_save_json_safe(data,path,"tmp","bak"))
        blog(LOG_WARNING,"MA OBS: could not save monitor display preference");
    obs_data_release(data);
    bfree(path);
}
static bool ma_read_diagnostics_preference() {return ma_read_monitor_preference("diagnostics",false);}
static bool ma_read_scene_list_preference() {return ma_read_monitor_preference("scene_list",true);}
static void ma_write_diagnostics_preference(bool value) {ma_write_monitor_preference("diagnostics",value);}
static void ma_write_scene_list_preference(bool value) {ma_write_monitor_preference("scene_list",value);}
static QString ma_audio_diagnostic_text();
#include "monitor_ui.h"
static QPointer<MonitorDock> monitor_dock;
}


// MA OBS 0.25: source/scene media rules, persistent configuration and monitor.
namespace {
#include "audio_control.h"
#include "video_control.h"
static QString ma_audio_diagnostic_text() {
    QStringList lines;
    lines.append(QString("Audio-Steuerung: %1 · Cue gültig: %2 · Fade: %3 %")
        .arg(ma_video.enabled ? "aktiv" : "aus").arg(ma_video.last_cue.valid ? "ja" : "nein")
        .arg(int(ma_video.last_cue.progress*100)));
    const auto gains=ma_video.audio_gains();
    for (const auto &kv:gains) {
        const auto uuid=kv.first.toUtf8();
        obs_source_t *source=obs_get_source_by_uuid(uuid.constData());
        if (!source) continue;
        int filters=0;
        obs_source_enum_filters(source,[](obs_source_t *,obs_source_t *filter,void *data) {
            const char *id=obs_source_get_unversioned_id(filter);
            if (id && std::strcmp(id,"ma_obs_runtime_audio_gain")==0)
                ++*static_cast<int*>(data);
        },&filters);
        lines.append(QString("%1 · MA-Pegel: %2 % · OBS-Lautstärke: %3 % · Stumm: %4 · Status: %5 · MA-Filter: %6")
            .arg(ma_utf(obs_source_get_name(source))).arg(int(kv.second*100))
            .arg(int(obs_source_get_volume(source)*100)).arg(obs_source_muted(source) ? "ja" : "nein")
            .arg(ma_video_media_state(obs_source_media_get_state(source))).arg(filters));
        obs_source_release(source);
    }
    if (gains.empty()) lines.append("Keine aktiven Audio-Regeln.");
    return lines.join('\n');
}
}

// MA OBS 0.21: EXPERIMENTAL channel-zero Program Pilot.
// No hidden switching loops: relinquishes control if OBS overwrites channel 0.
namespace {
struct ProgramPilot {
    obs_source_t *private_source = nullptr;  // our owned reference
    obs_source_t *previous_output = nullptr; // saved owned reference
    obs_source_t *program_transition = nullptr; // owned, private real OBS transition
    QTimer *status_timer = nullptr;
    bool active = false;
    bool replaced = false;
};
static ProgramPilot program_pilot;
static bool ma_media_control_active() {
    return program_pilot.active && !ma_obs_exiting;
}
static QTimer *ma_video_runtime_timer=nullptr;
static MAVideoCue ma_video_snapshot() {
    MAVideoCue cue;
    if (!program_pilot.active || !program_pilot.private_source) return cue;
    std::lock_guard<std::mutex> registry_lock(registry_mutex);
    for (Source *s : source_registry) {
        if (s->owner != program_pilot.private_source) continue;
        std::lock_guard<std::mutex> state_lock(s->state_mutex);
        if (!s->cue_initialized) break;
        cue.valid=true;
        cue.event=s->cue_events;
        cue.from_uuid=ma_utf(s->cue_from_uuid.c_str());
        cue.target_uuid=ma_utf(s->cue_to_uuid.c_str());
        cue.progress=s->cue_math.progress;
        cue.reached=s->cue_math.reached;
        cue.direct=s->cue_math.small_range;
        cue.live_layers=ma_live_frame_layers_locked(s);
        break;
    }
    return cue;
}


// A separate module-level preference: never change the OBS scene collection.
// Missing preference defaults to OFF; explicit menu actions persist immediately.


// Keep the OBS audio source tree activated for the selected live scene layers.
// Only the frontend thread mutates active_children. Holding references while
// the pilot runs is intentional; pilot_stop releases ALL of them before unload.
static void ma_pilot_sync_children() {
    if (!program_pilot.active || !program_pilot.private_source) return;
    std::lock_guard<std::mutex> registry_lock(registry_mutex);
    for (auto *s:source_registry) {
        if (s->owner!=program_pilot.private_source) continue;
        auto layers=ma_live_frame_layers(s);
        std::set<std::string> wanted;
        for (const auto &layer:layers) if (!layer.uuid.empty()) wanted.insert(layer.uuid);
        for (auto it=s->active_children.begin();it!=s->active_children.end();) {
            if (wanted.count(it->first)) {++it;continue;}
            obs_source_remove_active_child(s->owner,it->second);
            obs_source_release(it->second);
            it=s->active_children.erase(it);
        }
        for (const auto &uuid:wanted) {
            if (s->active_children.count(uuid)) continue;
            obs_source_t *child=obs_get_source_by_uuid(uuid.c_str());
            if (!child) continue;
            if (child != s->owner && obs_source_add_active_child(s->owner,child))
                s->active_children.emplace(uuid,child);
            else obs_source_release(child);
        }
        break;
    }
}
static void ma_pilot_clear_children() {
    std::lock_guard<std::mutex> registry_lock(registry_mutex);
    for (auto *s:source_registry) {
        if (s->owner!=program_pilot.private_source) continue;
        for (auto &item:s->active_children) {
            obs_source_remove_active_child(s->owner,item.second);
            obs_source_release(item.second);
        }
        s->active_children.clear();
        break;
    }
}
static std::string pilot_preference_path() {
    char *path = obs_module_config_path("program_pilot.json");
    if (!path) return {};
    std::string result(path);
    bfree(path);
    return result;
}
static bool read_pilot_preference() {
    const std::string path = pilot_preference_path();
    if (path.empty()) return false;
    obs_data_t *data = obs_data_create_from_json_file_safe(path.c_str(), "bak");
    if (!data) return false;
    const bool enabled = obs_data_get_bool(data, "enabled");
    obs_data_release(data);
    return enabled;
}
static void write_pilot_preference(bool enabled) {
    const std::string path = pilot_preference_path();
    if (path.empty()) return;
    obs_data_t *data = obs_data_create();
    obs_data_set_bool(data, "enabled", enabled);
    if (!obs_data_save_json_safe(data, path.c_str(), "tmp", "bak"))
        blog(LOG_WARNING, "MA OBS: could not persist Program Pilot preference");
    obs_data_release(data);
}


// v0.23: independent module-level settings. The private source no longer
// needs a visible template source. Existing collections and sources are untouched.
static std::string native_config_path() {
    char *path = obs_module_config_path("native_settings.json");
    if (!path) return {};
    std::string result(path);
    bfree(path);
    return result;
}
static obs_data_t *native_defaults() {
    obs_data_t *d = obs_data_create();
    obs_data_set_string(d, "control_type", "cue");
    obs_data_set_int(d, "universe", 12);
    obs_data_set_int(d, "address", 1);
    obs_data_set_string(d, "network_mode", "unicast");
    obs_data_set_string(d, "bind_ip", "0.0.0.0");
    obs_data_set_string(d, "multicast_interface", "192.168.1.71");
    obs_data_set_int(d, "width", 1920);
    obs_data_set_int(d, "height", 1080);
    obs_data_set_string(d, "startup_mode", "last");
    return d;
}
static obs_data_t *native_load() {
    const auto path = native_config_path();
    obs_data_t *d = path.empty() ? nullptr : obs_data_create_from_json_file_safe(path.c_str(), "bak");
    if (!d) return native_defaults();
    // Defaults supply fields missing from older configuration files.
    obs_data_set_default_string(d, "control_type", "cue");
    obs_data_set_default_int(d, "universe", 12);
    obs_data_set_default_int(d, "address", 1);
    obs_data_set_default_string(d, "network_mode", "unicast");
    obs_data_set_default_string(d, "bind_ip", "0.0.0.0");
    obs_data_set_default_string(d, "multicast_interface", "192.168.1.71");
    obs_data_set_default_int(d, "width", 1920);
    obs_data_set_default_int(d, "height", 1080);
    obs_data_set_default_string(d, "startup_mode", "last");
    return d;
}
static bool native_save(obs_data_t *d) {
    const auto path = native_config_path();
    if (path.empty()) return false;
    return obs_data_save_json_safe(d, path.c_str(), "tmp", "bak");
}
static bool should_autostart() {
    obs_data_t *d = native_load();
    const std::string mode = obs_data_get_string(d, "startup_mode");
    obs_data_release(d);
    if (mode == "on") return true;
    if (mode == "off") return false;
    return read_pilot_preference();
}
static void native_settings_dialog(void *) { ma_settings_dialog(); }

static void ma_add_native_pages(QDialog *dialog,QTabWidget *tabs,std::function<bool()> &save) {
    auto config=std::shared_ptr<obs_data_t>(native_load(),[](obs_data_t *d) {obs_data_release(d);});
    auto *general=new QWidget(tabs);
    auto *general_layout=new QVBoxLayout(general);
    auto *status=new QLabel(general); status->setWordWrap(true); general_layout->addWidget(status);
    auto *control=new QPushButton(general); general_layout->addWidget(control);
    const auto update_status=[status,control] {
        status->setText(ma_control_status_text());
        control->setText(pilot_ui_active.load() ? "MA-Steuerung deaktivieren" : "MA-Steuerung aktivieren");
    };
    QObject::connect(control,&QPushButton::clicked,dialog,[update_status] {
        pilot_toggle(nullptr); update_status();
    });
    auto *status_timer=new QTimer(dialog);
    QObject::connect(status_timer,&QTimer::timeout,dialog,update_status);
    status_timer->start(250); update_status();
    auto *form=new QFormLayout;
    auto *startup=new QComboBox(general);
    startup->addItem("Letzten Zustand wiederherstellen","last");
    startup->addItem("Immer aktivieren","on"); startup->addItem("Immer deaktivieren","off");
    const int startup_index=startup->findData(QString::fromUtf8(obs_data_get_string(config.get(),"startup_mode")));
    startup->setCurrentIndex(startup_index>=0 ? startup_index : 0);
    form->addRow("MA-Steuerung beim OBS-Start",startup);
    auto *width=new QSpinBox(general); width->setRange(16,8192);
    width->setValue(int(obs_data_get_int(config.get(),"width")));
    auto *height=new QSpinBox(general); height->setRange(16,8192);
    height->setValue(int(obs_data_get_int(config.get(),"height")));
    form->addRow("Ausgabebreite (Pixel)",width); form->addRow("Ausgabehöhe (Pixel)",height);
    general_layout->addLayout(form);
    auto *usage=new QLabel("MA OBS MediaLink verbindet grandMA3 mit der OBS-Ausgabe. "
        "Die MA steuert Szenenübergänge sowie die konfigurierten Video- und Audioregeln.\n\n"
        "Die Schaltfläche aktiviert/deaktiviert die Steuerung sofort; Abbrechen nimmt diese Aktion nicht zurück. "
        "Änderungen an Startverhalten und Ausgabeauflösung werden erst mit Speichern übernommen.",general);
    usage->setWordWrap(true); general_layout->addWidget(usage); general_layout->addStretch();
    tabs->addTab(general,"Allgemein");
    auto *connection=new QWidget(tabs);
    auto *connection_layout=new QVBoxLayout(connection);
    auto *network_form=new QFormLayout;
    auto *universe=new QSpinBox(connection); universe->setRange(1,63999);
    universe->setValue(int(obs_data_get_int(config.get(),"universe")));
    network_form->addRow("sACN Universum",universe);
    auto *address=new QSpinBox(connection); address->setRange(1,503);
    address->setValue(int(obs_data_get_int(config.get(),"address")));
    network_form->addRow("DMX Startadresse (10 Kanäle)",address);
    auto *network=new QComboBox(connection);
    network->addItem("Unicast","unicast"); network->addItem("Multicast","multicast");
    network->setCurrentIndex(std::strcmp(obs_data_get_string(config.get(),"network_mode"),"multicast")==0 ? 1 : 0);
    network_form->addRow("Empfangsmodus",network);
    auto *bind=new QLineEdit(QString::fromUtf8(obs_data_get_string(config.get(),"bind_ip")),connection);
    auto *iface=new QLineEdit(QString::fromUtf8(obs_data_get_string(config.get(),"multicast_interface")),connection);
    network_form->addRow("Unicast Bind-IP",bind);
    auto *bind_help=new QLabel("Lokale IPv4-Adresse des OBS-Rechners (Empfänger), nicht die IP der MA. "
        "0.0.0.0 empfängt auf allen lokalen IPv4-Adressen. In der MA ist die Ziel-IP die Adresse dieses OBS-Rechners.",connection);
    bind_help->setWordWrap(true); network_form->addRow("",bind_help);
    network_form->addRow("Multicast Interface-IP",iface);
    const auto update_network_fields=[=] {
        const bool multicast=network->currentData().toString()=="multicast";
        bind->setEnabled(!multicast);bind_help->setEnabled(!multicast);
        iface->setEnabled(multicast);
        if (auto *label=network_form->labelForField(bind)) label->setEnabled(!multicast);
        if (auto *label=network_form->labelForField(iface)) label->setEnabled(multicast);
        bind->setToolTip(multicast ? "Wird nur bei Unicast verwendet." : "Lokale IPv4-Adresse des OBS-Rechners.");
        iface->setToolTip(multicast ? "Lokale IPv4-Adresse der Netzwerkkarte für Multicast." : "Wird nur bei Multicast verwendet.");
    };
    QObject::connect(network,&QComboBox::currentIndexChanged,dialog,[=](int) {update_network_fields();});
    update_network_fields();
    connection_layout->addLayout(network_form);
    auto *hint=new QLabel("Verbindungs- und Auflösungsänderungen zuerst speichern. Sie gelten nach erneutem Aktivieren "
        "der MA-Steuerung. Das Startverhalten gilt beim nächsten OBS-Start.",connection);
    hint->setWordWrap(true); connection_layout->addWidget(hint);
    auto *notes=new QGroupBox("Cue-Note in grandMA3",connection);
    auto *note_layout=new QVBoxLayout(notes);
    auto *note_help=new QLabel("Format: OBS:<Szenennummer>, zum Beispiel OBS:1 oder OBS:12. "
        "Die Kennung OBS wird großgeschrieben. Doppelpunkt und Szenennummer folgen ohne Leerzeichen. "
        "Die Szenennummer wird ohne führende Nullen angegeben und entspricht der Position in der OBS-Szenenliste, beginnend bei 1.",notes);
    note_help->setTextFormat(Qt::PlainText);
    note_help->setWordWrap(true); note_layout->addWidget(note_help); connection_layout->addWidget(notes);
    connection_layout->addStretch();
    tabs->addTab(connection,"Verbindung");
    save=[=]() -> bool {
        const QString bind_ip=bind->text().trimmed(), interface_ip=iface->text().trimmed();
        in_addr parsed{};
        const QString active_ip=network->currentData().toString()=="multicast" ? interface_ip : bind_ip;
        const auto bytes=active_ip.toUtf8();
        if (InetPtonA(AF_INET,bytes.constData(),&parsed)!=1) {
            QMessageBox::warning(dialog,"MA OBS MediaLink","Bitte eine gültige IPv4-Adresse für den gewählten Empfangsmodus eingeben.");
            return false;
        }
        obs_data_set_int(config.get(),"universe",universe->value());
        obs_data_set_int(config.get(),"address",address->value());
        obs_data_set_int(config.get(),"width",width->value()); obs_data_set_int(config.get(),"height",height->value());
        obs_data_set_string(config.get(),"network_mode",network->currentData().toString().toUtf8().constData());
        obs_data_set_string(config.get(),"bind_ip",bind_ip.toUtf8().constData());
        obs_data_set_string(config.get(),"multicast_interface",interface_ip.toUtf8().constData());
        obs_data_set_string(config.get(),"startup_mode",startup->currentData().toString().toUtf8().constData());
        if (!native_save(config.get())) {
            QMessageBox::warning(dialog,"MA OBS MediaLink","Allgemeine Einstellungen konnten nicht gespeichert werden.");
            return false;
        }
        return true;
    };
}

// Called on the Qt / OBS frontend thread. The API returns the exact current
// Scenes dock order and requires its own release function (NOT per-scene releases).
// Only UUID + display-name strings are copied to the graphics thread.
static void refresh_pilot_scenes() {
    auto &p = program_pilot;
    if (!p.private_source) return;
    struct obs_frontend_source_list scenes = {};
    obs_frontend_get_scenes(&scenes);
    std::vector<Source::AutoScene> entries;
    entries.reserve(scenes.sources.num);
    for (size_t i = 0; i < scenes.sources.num; ++i) {
        obs_source_t *scene = scenes.sources.array[i];
        if (!scene) continue;
        const char *uuid = obs_source_get_uuid(scene);
        if (!uuid || !*uuid) continue;
        obs_source_t *latest = obs_get_source_by_uuid(uuid);
        const char *name = obs_source_get_name(latest ? latest : scene);
        if (name && *name) entries.push_back({uuid, name});
        if (latest) obs_source_release(latest);
    }
    obs_frontend_source_list_free(&scenes);
    std::lock_guard<std::mutex> registry_lock(registry_mutex);
    for (Source *s : source_registry) {
        if (s->owner != p.private_source) continue;
        std::lock_guard<std::mutex> lock(s->state_mutex);
        s->auto_scenes = std::move(entries);
        s->auto_scene_mode = true;
        // Keep currently rendering scenes by UUID on rename/reorder; only
        // update the on-screen display names. Next Cue uses the new order.
        for (const auto &entry : s->auto_scenes) {
            if (!s->cue_from_uuid.empty() && s->cue_from_uuid == entry.uuid)
                s->cue_from_name = entry.name;
            if (!s->cue_to_uuid.empty() && s->cue_to_uuid == entry.uuid)
                s->cue_to_name = entry.name;
        }
        break;
    }
}

static bool find_template_source(void *data, obs_source_t *source) {
    auto *found = static_cast<obs_source_t **>(data);
    const char *id = obs_source_get_unversioned_id(source);
    if (id && std::strcmp(id, "ma_sacn_crossfade") == 0) {
        *found = obs_source_get_ref(source);
        return false;
    }
    return true;
}

static void pilot_stop(void) {
    auto &p = program_pilot;
    // Close the command gate before any source/update signals can run.
    p.active = false;
    pilot_ui_active.store(false);
    pilot_ui_output_owned.store(false);
    ma_audio_gains.clear(); // no filter persists into OBS normal output
    ma_video.stop_pilot(); // drop video showing counts before the pilot releases scenes
    ma_pilot_clear_children(); // release scene refs BEFORE OBS switches collection
    if (p.status_timer) {
        p.status_timer->stop();
        p.status_timer->disconnect();
        p.status_timer->deleteLater();
        p.status_timer = nullptr;
    }
    if (p.private_source) {
        obs_source_t *actual = obs_get_output_source(0);
        const bool still_owner = actual == p.program_transition;
        if (actual) obs_source_release(actual);
        // Never overwrite a program channel that OBS has already reclaimed.
        if (still_owner) obs_set_output_source(0, p.previous_output);
        if (p.program_transition) {
            obs_transition_clear(p.program_transition);
            obs_source_release(p.program_transition);
            p.program_transition = nullptr;
        }
        obs_source_release(p.private_source);
        p.private_source = nullptr;
    }
    if (p.previous_output) {
        obs_source_release(p.previous_output);
        p.previous_output = nullptr;
    }
    p.active = false;
    p.replaced = false;
    pilot_ui_active.store(false);
    pilot_ui_output_owned.store(false);
    blog(LOG_INFO, "MA OBS Program Pilot: stopped");
}

static bool pilot_start(bool interactive) {
    if (ma_obs_exiting || !ma_frontend_available) return false;
    auto &p = program_pilot;
    if (p.active) return true;

    // Fully independent private source. No legacy output scene, playlist,
    // visible source or cue map is required by the Program Pilot.
    obs_data_t *settings = native_load();
    // Force Cue mode: OBS scene order is the sole number-to-scene mapping.
    obs_data_set_string(settings, "control_type", "cue");
    p.private_source = obs_source_create_private(
        "ma_sacn_crossfade", "MA OBS (private Program Pilot)", settings);
    obs_data_release(settings);
    if (!p.private_source) {
        if (interactive) QMessageBox::critical(nullptr, "MA OBS MediaLink", "Private Quelle konnte nicht erzeugt werden.");
        return false;
    }
    refresh_pilot_scenes();
    // OBS swaps ordinary frontend transitions using obs_transition_swap_begin/end.
    // Channel 0 MUST therefore contain a valid transition source, not our
    // OBS_SOURCE_TYPE_INPUT. v0.21 caused an access violation on UI transition changes.
    // A private CUT transition provides the required type without adding another fade.
    p.program_transition = obs_source_create_private(
        "cut_transition", "MA OBS private program wrapper", nullptr);
    if (!p.program_transition) {
        obs_source_release(p.private_source);
        p.private_source = nullptr;
        if (interactive) QMessageBox::critical(nullptr, "MA OBS MediaLink",
            "Der private Cut-Transition-Wrapper konnte nicht erstellt werden.");
        return false;
    }
    obs_transition_set(p.program_transition, p.private_source);
    p.previous_output = obs_get_output_source(0);
    if (!p.previous_output || obs_source_get_type(p.previous_output) != OBS_SOURCE_TYPE_TRANSITION) {
        if (p.previous_output) { obs_source_release(p.previous_output); p.previous_output = nullptr; }
        obs_transition_clear(p.program_transition);
        obs_source_release(p.program_transition); p.program_transition = nullptr;
        obs_source_release(p.private_source); p.private_source = nullptr;
        if (interactive) QMessageBox::critical(nullptr, "MA OBS MediaLink",
            "OBS Program ist keine Transition. Die MA-Steuerung bleibt ausgeschaltet.");
        return false;
    }
    obs_set_output_source(0, p.program_transition);
    p.active = true;
    p.replaced = false;
    pilot_ui_active.store(true);
    pilot_ui_output_owned.store(true);
    blog(LOG_WARNING, "MA OBS MA-Steuerung v0.45: EXPERIMENTAL transition-wrapper channel 0 active");

    p.status_timer = new QTimer();
    QObject::connect(p.status_timer, &QTimer::timeout, [] {
        auto &q = program_pilot;
        if (!q.active || !q.private_source) return;
        refresh_pilot_scenes();
        obs_source_t *actual = obs_get_output_source(0);
        const bool ours = actual == q.program_transition;
        if (actual) obs_source_release(actual);
        pilot_ui_output_owned.store(ours);
        if (!ours && !q.replaced) {
            q.replaced = true;
            blog(LOG_WARNING, "MA OBS Program Pilot: OBS replaced output channel 0. No forced reclaim.");
            // Do not fight OBS transition swaps: relinquish control safely.
            // The user's desired state stays ON and is restored on next launch.
            pilot_stop();
            blog(LOG_WARNING, "MA OBS: pilot relinquished output after OBS transition change; saved preference unchanged");
        }
    });
    p.status_timer->start(350);
    if (interactive) QMessageBox::information(nullptr, "MA OBS MediaLink",
        "Die MA-Steuerung der OBS-Ausgabe wurde aktiviert.\n\n"
        "OBS-Szenen werden automatisch anhand ihrer Position nummeriert.");
    return true;
}
static void pilot_toggle(void *) {
    if (ma_obs_exiting) return;
    if (program_pilot.active) {
        pilot_stop();
        write_pilot_preference(false);
    } else if (pilot_start(true)) {
        write_pilot_preference(true);
    }
}
// A collection switch unloads OBS scenes and transitions. Never keep the
// Program Pilot's previous output transition (or dock polling) across unload.
// The preference is intentionally NOT modified by an internal pilot_stop().
static bool collection_changing = false;
static uint64_t collection_epoch = 0;
static uint64_t prepared_collection_epoch = UINT64_MAX;
static uint64_t ma_current_collection_epoch() { return collection_epoch; }

static void schedule_collection_ready(int delay_ms, uint64_t epoch) {
    QTimer::singleShot(delay_ms, [epoch] {
        if (ma_obs_exiting || collection_changing || epoch != collection_epoch) return;
        if (monitor_dock) monitor_dock->resume_after_collection_change();
        if (ma_video.suspended || ma_video.collection != ma_video_collection_name()) ma_video.change_collection(false);
        if (prepared_collection_epoch!=epoch) {
            // Recover a collection saved while MA was active, even if the
            // previous OBS session crashed or startup mode is Always Off.
            if (!program_pilot.active) ma_restore_media_sources();
            prepared_collection_epoch=epoch;
        }
        if (should_autostart() && !program_pilot.active) {
            if (!pilot_start(false))
                blog(LOG_WARNING, "MA OBS: Program Pilot auto-start after collection change failed");
        }
    });
}

static void ma_shutdown_frontend() {
    if (ma_obs_exiting) return;
    ma_obs_exiting=true;
    collection_changing=true;
    ++collection_epoch; // invalidate every deferred startup callback
    if (ma_video_runtime_timer) {
        ma_video_runtime_timer->stop();
        delete ma_video_runtime_timer;
        ma_video_runtime_timer=nullptr;
    }
    if (monitor_dock)
        for (auto *timer:monitor_dock->findChildren<QTimer *>()) timer->stop();
    if (ma_settings_window) {
        for (auto *timer:ma_settings_window->findChildren<QTimer *>()) timer->stop();
        ma_settings_window->reject();
    }
    // SCRIPTING_SHUTDOWN precedes scene destruction. Never defer owned
    // source/transition references or showing counters to module unload.
    pilot_stop();
    ma_video.suspended=true;
    blog(LOG_INFO,"MA OBS: frontend shutdown complete; media settings restored and source references released");
}

static void ma_media_runtime_tick() {
    if (!ma_media_control_active() || collection_changing || ma_video.suspended) return;
    ma_pilot_sync_children();
    if (ma_video.enabled) {
        const auto cue=ma_video_snapshot();
        ma_video.tick(cue);
        ma_video.update_live_audio(cue);
        ma_audio_gains.apply(ma_video.audio_gains());
    } else ma_audio_gains.clear();
}

static void pilot_frontend_event(enum obs_frontend_event event, void *) {
    if (ma_obs_exiting && event!=OBS_FRONTEND_EVENT_EXIT) return;
    switch (event) {
    case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
        ma_shutdown_frontend();
        break;
    case OBS_FRONTEND_EVENT_EXIT:
        ma_shutdown_frontend();
        obs_frontend_remove_event_callback(pilot_frontend_event,nullptr);
        ma_frontend_available=false;
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
        ++collection_epoch; // invalidate any pending callbacks from previous collection
        collection_changing = true;
        if (monitor_dock) monitor_dock->pause_for_collection_change();
        // Release references to scenes and transitions before OBS unloads them.
        // Do not persist OFF: this is a temporary lifecycle shutdown.
        pilot_stop();
        ma_video.change_collection(true);
        blog(LOG_INFO, "MA OBS: collection changing: pilot released, dock polling paused");
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED: {
        collection_changing = false;
        const uint64_t epoch = ++collection_epoch;
        // Defer until OBS has populated the new scene collection.
        schedule_collection_ready(500, epoch);
        schedule_collection_ready(1500, epoch);
        break;
    }
    case OBS_FRONTEND_EVENT_FINISHED_LOADING: {
        collection_changing = false;
        const uint64_t epoch = ++collection_epoch;
        schedule_collection_ready(0, epoch);
        break;
    }
    default:
        break;
    }
}
}

bool obs_module_load(void) {
    obs_source_info info{};
    info.id = "ma_sacn_crossfade";
    info.type = OBS_SOURCE_TYPE_INPUT;
    // Keep the type for the private renderer and saved collections, but do not
    // offer the obsolete manually placed crossfade source in Add Source.
    info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE |
                        OBS_SOURCE_CAP_DISABLED;
    info.get_name = source_name;
    info.create = create;
    info.destroy = destroy;
    info.update = update;
    info.get_defaults = defaults;
    info.get_properties = properties;
    info.get_width = get_width;
    info.get_height = get_height;
    info.video_render = render;
    info.audio_render = ma_audio_render;
    info.enum_active_sources = ma_enum_active_sources;
    obs_register_source(&info);
    obs_source_info gain_info{};
    gain_info.id="ma_obs_runtime_audio_gain";
    gain_info.type=OBS_SOURCE_TYPE_FILTER;
    gain_info.output_flags=OBS_SOURCE_AUDIO;
    gain_info.get_name=ma_audio_gain_name;
    gain_info.create=ma_audio_gain_create;
    gain_info.destroy=ma_audio_gain_destroy;
    gain_info.update=ma_audio_gain_update;
    gain_info.save=ma_audio_gain_save;
    gain_info.filter_audio=ma_audio_gain_filter;
    obs_register_source(&gain_info);
    obs_frontend_add_tools_menu_item("MA OBS MediaLink…", native_settings_dialog, nullptr);
    ma_video.load();
    ma_video_runtime_timer = new QTimer();
    QObject::connect(ma_video_runtime_timer, &QTimer::timeout, ma_media_runtime_tick);
    ma_video_runtime_timer->start(50);
    obs_frontend_add_event_callback(pilot_frontend_event, nullptr);
    monitor_dock = new MonitorDock();
    if (!obs_frontend_add_dock_by_id("ma-sacn-crossfade-monitor", "MA OBS MediaLink · v0.45 BETA", monitor_dock.data())) {
        delete monitor_dock.data();
        monitor_dock = nullptr;
        blog(LOG_WARNING, "MA Crossfade: could not create monitor dock");
    }
    return true;
}

void obs_module_unload(void) {
    // After EXIT the frontend and sources no longer belong to us. Normal
    // shutdown already released everything in SCRIPTING_SHUTDOWN.
    if (ma_frontend_available) {
        ma_shutdown_frontend(); // also supports unload before normal OBS exit
        obs_frontend_remove_event_callback(pilot_frontend_event, nullptr);
        if (monitor_dock) obs_frontend_remove_dock("ma-sacn-crossfade-monitor");
    }
    monitor_dock = nullptr;
    ma_frontend_available=false;
}
