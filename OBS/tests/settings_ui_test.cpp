#include "../src/settings_policy.h"
#include <cassert>
#include <cmath>
#include <map>
#include <string>
#include <vector>
struct QString {
    std::string s;
    QString()=default;QString(const char *p):s(p){}
    bool operator==(const char *p)const{return s==p;}
};
QString ma_utf(const char *p){return QString(p);}
std::string ma_videokey(const QString &v){return v.s;}
struct QWidget {
    bool enabled=true,visible=true;int blocked=0;QString tooltip,text;
    void setEnabled(bool v){enabled=v;}
    void setVisible(bool v){visible=v;}
    void setToolTip(const QString &v){tooltip=v;}
    void setText(const QString &v){text=v;}
};
struct Check:QWidget{bool checked=true;bool isChecked()const{return checked;}};
struct Spin:QWidget{double number=0;double value()const{return number;}};
struct Combo:QWidget{QString mode="begin";bool require_block=false;};
struct QSignalBlocker {
    QWidget *p;
    explicit QSignalBlocker(QWidget *value):p(value){++p->blocked;}
    ~QSignalBlocker(){--p->blocked;}
};
QString getCombo(const Combo *p){return p->mode;}
void setCombo(Combo *p,const QString &value){if(p->require_block)assert(p->blocked);p->mode=value;}
struct Variant{bool value=false;bool toBool()const{return value;}};
namespace Qt{constexpr int UserRole=256;}
struct Item{
    bool live=false,ffmpeg=true;
    Variant data(int role)const{return {role==Qt::UserRole+1?live:ffmpeg};}
};
struct Media{Item *selected=nullptr;Item *currentItem(){return selected;}};
struct Form{
    std::map<QWidget *,QWidget> labels;
    QWidget *labelForField(QWidget *p){return &labels[p];}
};
struct Fixture {
    Media media_value;Media *media=&media_value;
    Form options_value;Form *options=&options_value;
    Item source;
    bool loading_rule=false;
    QString current_end_mode="black";
    QWidget percent_value,on_exit_value,reentry_value,on_abort_value,same_goto_value,shared_value,goto_mode_value;
    QWidget *percent=&percent_value,*on_exit=&on_exit_value,*reentry=&reentry_value,*on_abort=&on_abort_value;
    QWidget *same_goto=&same_goto_value,*shared=&shared_value,*goto_mode=&goto_mode_value;
    Combo start_value,on_end_value,on_end_basic_value,before_value,audio_value;
    Combo *start=&start_value,*on_end=&on_end_value,*on_end_basic=&on_end_basic_value,*before=&before_value,*audio_mode=&audio_value;
    Spin delay_value,expected_value,before_end_value;
    Spin *delay=&delay_value,*expected=&expected_value,*before_end=&before_end_value;
    Check prepare_value,enabled_value;
    Check *prepare=&prepare_value,*enabled=&enabled_value;
    QWidget prepare_info_value,display_info_value,timing_note_value,timing_error_value,apply_value;
    QWidget *prepare_info=&prepare_info_value,*display_info=&display_info_value,*timing_note=&timing_note_value;
    QWidget *timing_error=&timing_error_value,*apply=&apply_value;
    std::map<QWidget *,QString> original_tooltips;
    std::vector<QWidget *> video_controls{start,percent,delay,expected,before_end,on_exit,reentry,on_end,
        on_end_basic,before,on_abort,same_goto,shared,goto_mode};
    Fixture(){media->selected=&source;on_end->require_block=true;on_end_basic->require_block=true;}
#include "settings_ui_under_test.inc"
};
int main(){
    Fixture f;f.refresh();
    assert(f.on_end->visible && !f.on_end_basic->visible && f.before->enabled);
    assert(!f.percent->enabled && !f.delay->enabled && !f.expected->enabled && !f.before_end->enabled);
    assert(!f.options->labelForField(f.percent)->enabled && !f.percent->tooltip.s.empty());
    f.start->mode="percent";f.refresh();assert(f.percent->enabled && !f.delay->enabled);
    f.start->mode="delay";f.refresh();assert(f.delay->enabled && !f.percent->enabled);
    f.start->mode="before_end";f.expected->number=0;f.before_end->number=3;f.refresh();
    assert(f.expected->enabled && !f.before_end->enabled && f.timing_note->visible && f.apply->enabled);
    assert(f.before_end->number==3); // disabled values survive unchanged
    f.expected->number=2;f.refresh();assert(f.before_end->enabled && f.timing_error->visible && !f.apply->enabled);
    f.before_end->number=1;f.refresh();assert(!f.timing_error->visible && f.apply->enabled);
    f.start->mode="manual";f.refresh();
    assert(!f.same_goto->enabled && !f.goto_mode->enabled && !f.expected->enabled);
    assert(f.shared->enabled && f.reentry->enabled && f.on_abort->enabled);
    f.prepare->checked=false;f.refresh();
    assert(!f.on_end->visible && f.on_end_basic->visible && f.on_end_basic->enabled && !f.before->enabled);
    assert(f.display_info->visible && f.current_end_mode=="black" && f.on_end->mode=="black");
    f.prepare->checked=true;f.refresh();assert(f.on_end->mode=="black" && f.before->enabled);
    f.enabled->checked=false;f.refresh();
    for(auto *w:f.video_controls)assert(!w->enabled);
    assert(!f.audio_mode->enabled && !f.prepare->enabled && f.apply->enabled);
    f.enabled->checked=true;f.source.ffmpeg=false;f.refresh();
    assert(!f.prepare->enabled && !f.before->enabled && f.on_end_basic->enabled && f.audio_mode->enabled);
    f.source.live=true;f.refresh();
    for(auto *w:f.video_controls)assert(!w->visible);
    assert(f.audio_mode->enabled && !f.prepare->visible && !f.display_info->visible);
    f.media->selected=nullptr;f.refresh();assert(!f.apply->enabled && !f.audio_mode->enabled);
}
