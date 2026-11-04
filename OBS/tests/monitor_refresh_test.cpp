#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
struct QString {
    std::string s;
    QString()=default;QString(const char *v):s(v){}QString(std::string v):s(v){}
    static QString fromUtf8(const char *v) {return v;}
    static QString fromStdString(const std::string &v) {return v;}
    QString arg(const QString &value) const {
        auto result=s;size_t found=std::string::npos;int lowest=100;
        for(size_t i=0;i+1<s.size();++i) if(s[i]=='%' && s[i+1]>='0' && s[i+1]<='9') {
            int n=std::stoi(s.substr(i+1));if(n<lowest) {lowest=n;found=i;}
        }
        if(found!=std::string::npos) {
            size_t end=found+1;while(end<s.size() && s[end]>='0' && s[end]<='9') ++end;
            result.replace(found,end-found,value.s);
        }
        return result;
    }
    QString arg(const char *value) const {return arg(QString(value));}
    template<class T> QString arg(T value) const {return arg(QString(std::to_string(value)));}
    bool operator!=(const QString &v) const {return s!=v.s;}
};
static int locks=0;
static bool ma_obs_exiting=false;
struct TestGuard {
    std::lock_guard<std::mutex> guard;
    explicit TestGuard(std::mutex &m):guard(m) {++locks;}
    ~TestGuard() {--locks;}
};
struct QLabel {
    QString content,style;std::vector<std::string> writes;
    void setText(const QString &s) {assert(locks==0);content=s;writes.push_back(s.s);}
    QString text() const {return content;}
    QString styleSheet() const {return style;}
    void setStyleSheet(const QString &s) {assert(locks==0);style=s;}
};
struct QPushButton:QLabel {};
struct QProgressBar {
    int current=-1;std::vector<int> writes;
    void setValue(int v) {assert(locks==0);current=v;writes.push_back(v);}
    int value() const {return current;}
};
struct Receiver {
    struct CueFrame {uint32_t raw=42,scene=6,endpoint=100,token=4;uint64_t frame_serial=9;};
    std::atomic<uint64_t> last_packet_ms{900};std::atomic<bool> socket_bound{true};
    std::atomic<int> last_socket_error{0},level{42};std::atomic<uint32_t> sender_ipv4{1};
    std::mutex frame_mutex;CueFrame cue_frame;
    int universe=12,address=1;bool multicast=true;std::string bind_ip="0.0.0.0",multicast_interface="192.168.1.1";
};
struct obs_source_t {std::string name;};
const char *obs_source_get_name(obs_source_t *s) {return s->name.c_str();}
uint64_t GetTickCount64() {return 1000;}
std::string ipv4_string(uint32_t) {return "192.168.1.2";}
struct Source {
    obs_source_t *owner;Receiver receiver;bool cue_mode=true,cue_initialized=true;
    std::mutex state_mutex;
    struct Entry {std::string uuid,name;};std::vector<Entry> auto_scenes;
    struct Math {bool reached=false;float progress=.5f;} cue_math;
    std::string cue_to_uuid="b",cue_to_name="target",cue_from_uuid="a",cue_from_name="from";
    uint32_t cue_last_scene=6,cue_last_token=4,cue_last_endpoint=100;uint64_t cue_events=2;
    bool cue_from_frozen=false;std::vector<int> live_base;
};
static std::mutex registry_mutex;static std::vector<Source*> source_registry;
static std::atomic<bool> pilot_ui_active{true};
QString ma_control_status_text() {return pilot_ui_active.load()?"AKTIV":"AUS";}
struct Dock {
    QLabel program,pilot,target,signal,conn,cur,prep,bank;QPushButton control;QProgressBar progress;
    QLabel *program_label=&program,*pilot_status_label=&pilot,*target_label=&target,*signal_status=&signal;
    QLabel *connection=&conn,*current=&cur,*prepared=&prep,*banks=&bank;
    QPushButton *control_button=&control;QProgressBar *fade_progress=&progress;
#include "monitor_refresh_under_test.inc"
};
int main() {
    obs_source_t owner{"MA OBS (private Program Pilot)"};Source source;source.owner=&owner;
    source.auto_scenes={{"a","from"},{"b","target"}};source_registry={&source};Dock dock;
    dock.refresh();assert(dock.program.text().s=="Program: OBS:1 · from");
    assert(dock.progress.writes==std::vector<int>({500}));
    dock.refresh();assert(dock.program.writes.size()==1 && dock.progress.writes.size()==1);
    source.cue_math.progress=.75f;dock.refresh();
    assert(dock.progress.writes==std::vector<int>({500,750}));
    assert(dock.program.writes.size()==1); // never off between real snapshots
    source.cue_math.reached=true;source.cue_math.progress=1;dock.refresh();
    assert(dock.program.text().s=="Program: OBS:2 · target" && dock.progress.value()==1000);
    source.receiver.last_packet_ms=0;dock.refresh();assert(dock.signal.styleSheet().s.size()>0);
    assert(dock.progress.value()==1000 && dock.program.writes.size()==2);
    source.cue_initialized=false;dock.refresh();assert(dock.program.text().s=="Program: Warte auf erste OBS-Cue");
    assert(dock.progress.value()==0);
    source_registry.clear();pilot_ui_active=false;dock.refresh();
    assert(dock.program.text().s=="Program: MA-Steuerung ausgeschaltet");
    assert(dock.conn.text().s=="Keine aktive MA-Steuerung" && dock.cur.text().s.empty());
    assert(locks==0);
}
