// MA OBS MediaLink v0.45 BETA. Frontend-thread display helpers only.
#pragma once

static QString ma_ui_state_path() {
    char *path=obs_module_config_path("ui_state.ini");
    const QString result=QString::fromUtf8(path ? path : "");
    bfree(path);
    return result;
}

static QIcon ma_media_icon(const QString &operation) {
    // Painted geometry has no dependency on Unicode glyphs or installed fonts.
    // Supply multiple sizes for high-DPI displays and explicit disabled artwork.
    static std::map<QString,QIcon> cache;
    auto found=cache.find(operation);
    if (found!=cache.end()) return found->second;
    QIcon icon;
    for (const int size : {16,24,32,48,64}) {
        for (const bool disabled : {false,true}) {
            QPixmap pixmap(size,size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.scale(size/24.0,size/24.0);
            const QColor color=disabled ? QColor(135,135,135) : QColor(245,245,245);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            if (operation=="play") {
                QPainterPath path;
                path.moveTo(7,4); path.lineTo(20,12); path.lineTo(7,20); path.closeSubpath();
                painter.drawPath(path);
            } else if (operation=="enabled") {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(disabled ? color : QColor(91,210,123),3,
                    Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                QPainterPath check;
                check.moveTo(4,12); check.lineTo(10,18); check.lineTo(21,5);
                painter.drawPath(check);
            } else if (operation=="pause") {
                painter.drawRect(QRectF(6,4,4,16));
                painter.drawRect(QRectF(14,4,4,16));
            } else if (operation=="stop") {
                painter.drawRect(QRectF(5,5,14,14));
            } else {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(color,2.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
                painter.drawArc(QRectF(5,5,14,14),45*16,-285*16);
                painter.setPen(Qt::NoPen); painter.setBrush(color);
                QPainterPath arrow;
                arrow.moveTo(15,2); arrow.lineTo(15,10); arrow.lineTo(21,6); arrow.closeSubpath();
                painter.drawPath(arrow);
            }
            painter.end();
            icon.addPixmap(pixmap,disabled ? QIcon::Disabled : QIcon::Normal);
        }
    }
    cache.emplace(operation,icon);
    return icon;
}

static QString ma_media_action_name(const QString &operation) {
    if (operation=="play") return "Abspielen";
    if (operation=="restart") return "Neu starten";
    if (operation=="pause") return "Pausieren";
    return "Stoppen";
}

static QString ma_control_status_text() {
    if (pilot_ui_active.load() && pilot_ui_output_owned.load())
        return "MA-Steuerung: AKTIV · MA steuert die OBS-Ausgabe";
    if (pilot_ui_active.load())
        return "MA-Steuerung: OBS-Ausgabe nicht mehr unter MA-Kontrolle";
    return "MA-Steuerung: AUS";
}
