// Live media controls. Included after MAVideoEngine and the settings dialog.
#pragma once
class MAVideoDockPanel : public QWidget {
    struct Row {
        QWidget *widget=nullptr;
        QLabel *label=nullptr;
        QPushButton *toggle=nullptr, *restart=nullptr, *stop=nullptr;
    };
    QLabel *heading=nullptr;
    QVBoxLayout *items=nullptr;
    QTimer *timer=nullptr;
    std::map<QString,Row> rows;

    Row create_row(const QString &uuid) {
        Row row;
        row.widget=new QWidget(this);
        auto *layout=new QHBoxLayout(row.widget); layout->setContentsMargins(0,2,0,2);
        row.label=new QLabel(row.widget); row.label->setWordWrap(true); layout->addWidget(row.label,1);
        auto button=[&](const QString &operation) {
            auto *control=new QPushButton(row.widget);
            control->setFixedSize(28,28); control->setIconSize(QSize(18,18));
            control->setAutoDefault(false);
            if (operation=="toggle") {
                control->setCheckable(true);
                control->setStyleSheet(
                    "QPushButton:checked { background-color:#278540; border:1px solid #63c77e; border-radius:3px; }"
                    "QPushButton:checked:hover { background-color:#319d4d; }");
            } else {
                control->setIcon(ma_media_icon(operation));
                control->setToolTip(ma_media_action_name(operation));
            }
            // Always resolve Toggle against OBS state at click time, not against
            // the button's checked state or a stale snapshot from the UI timer.
            QObject::connect(control,&QPushButton::clicked,this,[this,uuid,operation] {
                ma_video.manual(uuid,operation); populate();
            });
            layout->addWidget(control);
            return control;
        };
        row.toggle=button("toggle"); row.restart=button("restart"); row.stop=button("stop");
        return row;
    }
    void populate() {
        if (ma_obs_exiting) return;
        if (!items) return;
        const bool active=ma_video.enabled && !ma_video.suspended;
        std::set<QString> wanted;
        if (active) for (const auto &pending:ma_video.pending) wanted.insert(pending.source);
        // Do not recreate buttons every poll: deleting a pressed button can
        // swallow its release/click event. Remove only sources that have left.
        for (auto it=rows.begin();it!=rows.end();) {
            if (wanted.count(it->first)) {++it;continue;}
            items->removeWidget(it->second.widget);
            delete it->second.widget;
            it=rows.erase(it);
        }
        if (!active) {
            heading->setText(ma_video.suspended ? "Medien: Szenensammlung wird gewechselt…" :
                "Video- und Audiosteuerung: AUS (in Einstellungen aktivieren)");
            return;
        }
        heading->setText(QString("Medien der Zielszene · %1 Quelle(n)").arg(wanted.size()));
        const bool diagnostic=property("ma_diagnostics").toBool();
        std::set<QString> displayed;
        int index=0;
        for (const auto &pending:ma_video.pending) {
            if (!displayed.insert(pending.source).second) continue;
            auto found=rows.find(pending.source);
            if (found==rows.end()) found=rows.emplace(pending.source,create_row(pending.source)).first;
            auto &row=found->second;
            if (items->indexOf(row.widget)!=index) items->insertWidget(index,row.widget);
            ++index;
            const auto bytes=pending.source.toUtf8();
            obs_source_t *source=obs_get_source_by_uuid(bytes.constData());
            const bool available=source && ma_media_source(source);
            const QString name=source ? ma_utf(obs_source_get_name(source)) : QString("Quelle gelöscht");
            const obs_media_state state=available ? obs_source_media_get_state(source) : OBS_MEDIA_STATE_NONE;
            const bool playing=available && ma_media_is_playing(state);
            const bool can_pause=available && ma_media_can_pause(state);
            QString status="Nicht vorhanden";
            if (available) {
                status=ma_video_media_state(state)+" · "+ma_video_ms(obs_source_media_get_time(source))+
                    " / "+ma_video_ms(obs_source_media_get_duration(source));
                if (!pending.fired && !pending.manual_override && !ma_media_can_pause(state)) {
                    QString waiting="Wartet auf Start";
                    if (pending.rule.start=="end") waiting="Wartet auf Fadeende";
                    else if (pending.rule.start=="manual") waiting="Manueller Start";
                    else if (pending.rule.start=="percent") waiting=QString("Wartet auf %1 %").arg(pending.rule.percent);
                    else if (pending.rule.start=="delay") waiting="Startverzögerung";
                    status=waiting+" · "+status;
                }
            }
            if (source) obs_source_release(source);
            if (diagnostic && pending.manual_override) status+=" · Manuelle Steuerung";
            row.label->setText(name+"\n"+status);
            {
                QSignalBlocker blocker(row.toggle);
                row.toggle->setChecked(playing); // solely the observed OBS state
            }
            row.toggle->setIcon(ma_media_icon(can_pause ? "pause" : "play"));
            const QString action=can_pause ? "Pausieren" : "Abspielen";
            row.toggle->setToolTip(action+(playing ? " · Wiedergabe läuft" : ""));
            row.toggle->setAccessibleName(action+": "+name);
            row.restart->setAccessibleName("Neu starten: "+name);
            row.stop->setAccessibleName("Stoppen: "+name);
            row.toggle->setEnabled(available); row.restart->setEnabled(available); row.stop->setEnabled(available);
        }
    }
public:
    explicit MAVideoDockPanel(QWidget *parent=nullptr):QWidget(parent) {
        auto *root=new QVBoxLayout(this); root->setContentsMargins(0,4,0,0);
        heading=new QLabel("Medien",this); heading->setStyleSheet("font-weight:bold;"); root->addWidget(heading);
        auto *scroll=new QScrollArea(this); scroll->setWidgetResizable(true); scroll->setMaximumHeight(195);
        auto *inner=new QWidget(scroll); items=new QVBoxLayout(inner); items->setContentsMargins(0,0,0,0);
        items->setSpacing(0); items->setAlignment(Qt::AlignTop); scroll->setWidget(inner); root->addWidget(scroll);
        auto *config=new QPushButton("Video und Audio konfigurieren…",this);
        QObject::connect(config,&QPushButton::clicked,this,[] {ma_settings_dialog(2);}); root->addWidget(config);
        timer=new QTimer(this);
        QObject::connect(timer,&QTimer::timeout,this,[this] {populate();}); timer->start(150);
        populate();
    }
};
static QWidget *ma_create_video_panel(QWidget *parent) { return new MAVideoDockPanel(parent); }
