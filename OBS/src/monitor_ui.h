// MA OBS MediaLink v0.46 BETA. Included inside plugin.cpp's namespace.
#pragma once
class MonitorDock final : public QWidget {
    QLabel *connection=nullptr, *program_label=nullptr, *pilot_status_label=nullptr;
    QLabel *current=nullptr, *prepared=nullptr, *banks=nullptr;
    QLabel *signal_status=nullptr, *target_label=nullptr;
    QPlainTextEdit *scene_mapping=nullptr;
    QProgressBar *fade_progress=nullptr;
    QTimer *timer=nullptr, *scene_timer=nullptr;
    QWidget *diagnostics=nullptr, *video_panel=nullptr, *scene_list=nullptr;
    QPushButton *control_button=nullptr;

    void set_diagnostics_visible(bool visible,bool persist=true) {
        diagnostics->setVisible(visible);
        if (video_panel) video_panel->setProperty("ma_diagnostics",visible);
        if (persist) ma_write_diagnostics_preference(visible);
    }
    void set_scene_list_visible(bool visible,bool persist=true) {
        scene_list->setVisible(visible);
        if (persist) ma_write_scene_list_preference(visible);
    }
    void refresh_scene_list() {
        if (ma_obs_exiting) return;
        obs_frontend_source_list scenes{};
        obs_frontend_get_scenes(&scenes);
        QStringList rows;
        for (size_t i=0;i<scenes.sources.num;++i) {
            obs_source_t *scene=scenes.sources.array[i];
            if (!scene) continue;
            const char *uuid=obs_source_get_uuid(scene);
            obs_source_t *latest=uuid ? obs_get_source_by_uuid(uuid) : nullptr;
            const char *name=obs_source_get_name(latest ? latest : scene);
            rows.append(QString("OBS:%1  %2").arg(i+1)
                .arg(QString::fromUtf8(name ? name : "")));
            if (latest) obs_source_release(latest);
        }
        obs_frontend_source_list_free(&scenes);
        const QString text=rows.isEmpty() ? QString("Keine OBS-Szenen") : rows.join('\n');
        if (text==scene_mapping->toPlainText()) return;
        const int v=scene_mapping->verticalScrollBar()->value();
        const int h=scene_mapping->horizontalScrollBar()->value();
        const QTextCursor cursor=scene_mapping->textCursor();
        QSignalBlocker blocker(scene_mapping);
        scene_mapping->setPlainText(text);
        scene_mapping->setTextCursor(cursor);
        scene_mapping->verticalScrollBar()->setValue(v);
        scene_mapping->horizontalScrollBar()->setValue(h);
    }
    QString diagnostic_text() const {
        return QString("MA OBS MediaLink v0.46 BETA\n%1\n%2\n%3\n%4\n\nSzenenzuordnung:\n%5\n\n%6\n%7\n%8\n%9")
            .arg(program_label->text()).arg(pilot_status_label->text()).arg(signal_status->text())
            .arg(target_label->text()).arg(scene_mapping->toPlainText()).arg(connection->text())
            .arg(current->text()).arg(prepared->text()).arg(banks->text())+"\n\n"+ma_audio_diagnostic_text();
    }
    void refresh() {
        if (ma_obs_exiting) return;
        // Build the entire view before touching widgets. QProgressBar can
        // repaint synchronously; intermediate off/zero setters caused flicker.
        struct View {
            QString program="Program: MA-Steuerung ausgeschaltet", target="Ziel: –";
            QString signal="MA-Signal: Steuerung ausgeschaltet", signal_style;
            QString connection="Keine aktive MA-Steuerung", current, prepared, banks;
            int progress=0;
        };
        const View view=[] {
            View view;
            std::lock_guard<std::mutex> registry_lock(registry_mutex);
            Source *selected=nullptr;
            for (Source *source:source_registry) {
                const char *name=obs_source_get_name(source->owner);
                // Keep the private internal identifier for compatibility. It is
                // never exposed as a selectable source in the MediaLink UI.
                if (name && std::strcmp(name,"MA OBS (private Program Pilot)")==0) {
                    selected=source; break;
                }
            }
            if (!selected) {
                return view;
            }
            auto &receiver=selected->receiver;
            const uint64_t last=receiver.last_packet_ms.load(), now=uint64_t(GetTickCount64());
            const bool receiving=receiver.socket_bound.load() && last && now>=last && now-last<2500;
            view.signal=receiving ? "MA-Signal: Empfang aktiv" : "MA-Signal: Kein Empfang – Diagnose öffnen";
            view.signal_style=receiving ? "" : "font-weight:bold; color:#e5a147;";
            Receiver::CueFrame frame{};
            if (selected->cue_mode) {
                std::lock_guard<std::mutex> lock(receiver.frame_mutex);
                frame=receiver.cue_frame;
            }
            const int error=receiver.last_socket_error.load();
            const QString state=receiving ? "Empfang aktiv" : receiver.socket_bound.load()
                ? "Signal verloren / kein Empfang" : "Socket nicht verfügbar";
            view.connection=QString("sACN U%1 · %2 · %3\nDMX %4 · Sender %5%6\nBind-IP: %7 · Multicast-Interface: %8\nLetztes Paket: %9")
                .arg(receiver.universe).arg(receiver.multicast ? "Multicast" : "Unicast").arg(state)
                .arg(selected->cue_mode ? frame.raw : uint32_t(receiver.level.load()))
                .arg(QString::fromStdString(ipv4_string(receiver.sender_ipv4.load())))
                .arg(error ? QString(" · Socket-Fehler %1").arg(error) : QString())
                .arg(QString::fromStdString(receiver.bind_ip)).arg(QString::fromStdString(receiver.multicast_interface))
                .arg(last && now>=last ? QString("vor %1 ms").arg(now-last) : QString("noch keines"));
            std::lock_guard<std::mutex> state_lock(selected->state_mutex);
            const auto describe=[selected](const std::string &uuid,const std::string &fallback) {
                for (size_t i=0;i<selected->auto_scenes.size();++i) {
                    const auto &entry=selected->auto_scenes[i];
                    if (entry.uuid==uuid) return QString("OBS:%1 · %2").arg(i+1)
                        .arg(QString::fromUtf8(entry.name.c_str()));
                }
                return QString::fromUtf8(fallback.c_str());
            };
            if (!selected->cue_initialized) view.program="Program: Warte auf erste OBS-Cue";
            else {
                const bool reached=selected->cue_math.reached;
                view.program=QString("Program: %1").arg(reached
                    ? describe(selected->cue_to_uuid,selected->cue_to_name)
                    : describe(selected->cue_from_uuid,selected->cue_from_name));
                view.target=reached ? QString("Ziel erreicht") : QString("Ziel: %1")
                    .arg(describe(selected->cue_to_uuid,selected->cue_to_name));
                view.progress=int(std::clamp(selected->cue_math.progress,0.f,1.f)*1000);
            }
            view.current=QString("Cue-Modus: Szene %1 · Token %2 · Events %3")
                .arg(selected->cue_last_scene).arg(selected->cue_last_token).arg(selected->cue_events);
            const auto bytes24=[](uint32_t v) {
                return QString("%1/%2/%3").arg((v>>16)&255u).arg((v>>8)&255u).arg(v&255u);
            };
            const auto bytes16=[](uint16_t v) {
                return QString("%1/%2").arg((v>>8)&255u).arg(v&255u);
            };
            view.prepared=QString("Ziel: %1 · Endwert: %2\nEmpfangen: Szene %3 · Endwert %4 · Token %5 · Frame %6\nDMX-Bytes ab Startadresse: %7 | %8 | %9 | %10")
                .arg(QString::fromUtf8(selected->cue_to_name.c_str())).arg(selected->cue_last_endpoint)
                .arg(frame.scene).arg(frame.endpoint).arg(frame.token).arg(frame.frame_serial)
                .arg(bytes24(frame.raw)).arg(bytes16(frame.scene)).arg(bytes24(frame.endpoint)).arg(bytes16(frame.token));
            view.banks=QString("Fade: %1 % · Startbild: %2%3\nDMX Startadresse: %4 · Live-Ebenen: %5")
                .arg(int(selected->cue_math.progress*100)).arg(QString::fromUtf8(selected->cue_from_name.c_str()))
                .arg(selected->cue_from_frozen ? " (Snapshot)" : " (Live)")
                .arg(receiver.address).arg(selected->live_base.size());
            return view;
        }(); // All OBS locks have been released before any Qt setter below.
        const auto text=[](QLabel *label,const QString &value) {
            if (label->text()!=value) label->setText(value);
        };
        text(pilot_status_label,ma_control_status_text());
        if (control_button) control_button->setText(pilot_ui_active.load()
            ? "MA-Steuerung deaktivieren" : "MA-Steuerung aktivieren");
        text(program_label,view.program); text(target_label,view.target);
        text(signal_status,view.signal);
        if (signal_status->styleSheet()!=view.signal_style)
            signal_status->setStyleSheet(view.signal_style);
        text(connection,view.connection); text(current,view.current);
        text(prepared,view.prepared); text(banks,view.banks);
        if (fade_progress->value()!=view.progress) fade_progress->setValue(view.progress);
    }

public:
    void pause_for_collection_change() {
        timer->stop(); scene_timer->stop();
        pilot_status_label->setText("Szenensammlung wird gewechselt …");
        if (control_button) control_button->setEnabled(false);
    }
    void resume_after_collection_change() {
        refresh_scene_list(); refresh();
        if (control_button) control_button->setEnabled(true);
        timer->start(150); scene_timer->start(1000);
    }
    explicit MonitorDock(bool diagnostic_only=false,QWidget *parent=nullptr):QWidget(parent) {
        auto *outer=new QVBoxLayout(this); outer->setContentsMargins(4,4,4,4);
        auto *scroll=new QScrollArea(this);
        scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        scroll->setMinimumSize(0,0);
        scroll->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
        auto *content=new QWidget(scroll);
        auto *layout=new QVBoxLayout(content);
        layout->setContentsMargins(2,2,2,2);
        layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
        QCheckBox *mode=nullptr,*scenes_mode=nullptr;
        if (!diagnostic_only) {
            auto *toolbar=new QHBoxLayout;
            mode=new QCheckBox("Diagnose",content); toolbar->addWidget(mode);
            scenes_mode=new QCheckBox("Szenenliste",content); toolbar->addWidget(scenes_mode);
            toolbar->addStretch();
            auto *settings=new QPushButton("Einstellungen…",content); toolbar->addWidget(settings);
            layout->addLayout(toolbar);
            control_button=new QPushButton(content); layout->addWidget(control_button);
            QObject::connect(settings,&QPushButton::clicked,this,[] {ma_settings_dialog();});
            QObject::connect(control_button,&QPushButton::clicked,this,[] {pilot_toggle(nullptr);});
        }
        program_label=new QLabel(content); program_label->setStyleSheet("font-weight:bold;");
        pilot_status_label=new QLabel(content); signal_status=new QLabel(content); target_label=new QLabel(content);
        fade_progress=new QProgressBar(content); fade_progress->setRange(0,1000); fade_progress->setFormat("Fade: %p %");
        for (auto *label:{program_label,pilot_status_label,signal_status,target_label}) {
            label->setWordWrap(true); layout->addWidget(label);
        }
        layout->addWidget(fade_progress);
        if (!diagnostic_only) {
            video_panel=ma_create_video_panel(content); layout->addWidget(video_panel);
        }
        auto *scenes_box=new QGroupBox("OBS-Szenennummern",content);scene_list=scenes_box;
        auto *scenes_layout=new QVBoxLayout(scenes_box);
        scene_mapping=new QPlainTextEdit(scenes_box); scene_mapping->setReadOnly(true);
        scene_mapping->setMinimumHeight(100); scene_mapping->setMaximumHeight(160);
        scenes_layout->addWidget(scene_mapping);layout->addWidget(scenes_box);
        auto *box=new QGroupBox("Diagnose · DMX und Netzwerk",content); diagnostics=box;
        auto *details=new QVBoxLayout(box);
        connection=new QLabel(box); current=new QLabel(box); prepared=new QLabel(box); banks=new QLabel(box);
        for (auto *label:{connection,current,prepared,banks}) {
            label->setWordWrap(true); details->addWidget(label);
        }
        auto *copy=new QPushButton("Diagnose kopieren",box); details->addWidget(copy);
        QObject::connect(copy,&QPushButton::clicked,this,[this] {
            QApplication::clipboard()->setText(diagnostic_text());
        });
        layout->addWidget(box); layout->addStretch();
        scroll->setWidget(content); outer->addWidget(scroll);
        if (mode) {
            const bool show=ma_read_diagnostics_preference();
            mode->setChecked(show);set_diagnostics_visible(show,false);
            QObject::connect(mode,&QCheckBox::toggled,this,[this](bool visible) {
                set_diagnostics_visible(visible);
            });
        }
        if (scenes_mode) {
            const bool show=ma_read_scene_list_preference();
            scenes_mode->setChecked(show);set_scene_list_visible(show,false);
            QObject::connect(scenes_mode,&QCheckBox::toggled,this,[this](bool visible) {
                set_scene_list_visible(visible);
            });
        }
        timer=new QTimer(this);
        QObject::connect(timer,&QTimer::timeout,this,[this] {refresh();}); timer->start(150);
        scene_timer=new QTimer(this);
        QObject::connect(scene_timer,&QTimer::timeout,this,[this] {refresh_scene_list();}); scene_timer->start(1000);
        refresh_scene_list(); refresh();
    }
};
static QWidget *ma_create_diagnostic_panel(QWidget *parent) { return new MonitorDock(true,parent); }
