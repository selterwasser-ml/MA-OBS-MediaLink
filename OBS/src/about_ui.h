// Included inside plugin.cpp's namespace; image bytes are embedded by Qt rcc.
#pragma once
#include "about_secret.h"
static void ma_open_local_package(QWidget *parent) {
    char *path=obs_module_file("ma-paket");
    const QString folder=QString::fromUtf8(path?path:"");
    bfree(path);
    if (folder.isEmpty() || !QDir(folder).exists()) {
        QMessageBox::information(parent,"MA OBS MediaLink",
            "Das lokale MA-Paket wurde nicht gefunden. Bitte das aktuelle MediaLink-Setup installieren.");
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
        QMessageBox::warning(parent,"MA OBS MediaLink","Der MA-Paketordner konnte nicht geöffnet werden.\n"+folder);
}

static void ma_add_about_identity(QWidget *about,QVBoxLayout *layout) {
    auto *logo=new QLabel(about);
    const QPixmap image(QStringLiteral(":/ma-obs-medialink/medialink-logo.png"));
    logo->setPixmap(image.scaled(160,140,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    logo->setAccessibleName("MA OBS MediaLink Logo");
    layout->addWidget(logo,0,Qt::AlignLeft);
}

static void ma_add_repository_button(QWidget *about,QVBoxLayout *layout) {
    auto *repository=new QPushButton("GitHub-Repository öffnen",about);
    QObject::connect(repository,&QPushButton::clicked,about,[about] {
        if (!QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/selterwasser-ml/MA-OBS-MediaLink"))))
            QMessageBox::warning(about,"MA OBS MediaLink","Das GitHub-Repository konnte nicht im Browser geöffnet werden.");
    });
    layout->addWidget(repository);
}

class MACatPhotoView : public QWidget {
    QPixmap photo{QStringLiteral(":/ma-obs-medialink/cats_farin_bela.png")};
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QSize size=photo.size();size.scale(contentsRect().size(),Qt::KeepAspectRatio);
        const QRect area(QPoint((width()-size.width())/2,(height()-size.height())/2),size);
        painter.drawPixmap(area,photo);
    }
public:
    explicit MACatPhotoView(QWidget *parent):QWidget(parent) {
        setMinimumSize(160,180);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        setAccessibleName("Farin und Bela");
    }
};
static QLabel *ma_create_dedication(QWidget *parent) {
    auto *label=new QLabel(parent);
    const QString color=label->palette().color(QPalette::WindowText).name();
    label->setText(QString("Für <a href=\"farin\" style=\"color:%1; text-decoration:none;\">Farin</a> und "
        "<a href=\"bela\" style=\"color:%1; text-decoration:none;\">Bela</a>.").arg(color));
    label->setTextFormat(Qt::RichText);
    label->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    label->setOpenExternalLinks(false);
    label->setStyleSheet("font-style:italic; margin-top:16px;");
    QObject::connect(label,&QLabel::linkActivated,parent,
        [parent,secret=MAAboutSecret{},window=QPointer<QDialog>{}](const QString &word) mutable {
            if (ma_obs_exiting || !secret.click(word.toStdString())) return;
            if (window) {window->show();window->raise();window->activateWindow();return;}
            auto *photo=new QDialog(parent->window());window=photo;
            photo->setAttribute(Qt::WA_DeleteOnClose);
            photo->setWindowTitle("Farin und Bela");
            photo->setWindowModality(Qt::WindowModal);
            auto *layout=new QVBoxLayout(photo);
            layout->addWidget(new MACatPhotoView(photo),1);
            auto *close=new QDialogButtonBox(QDialogButtonBox::Close,photo);
            close->button(QDialogButtonBox::Close)->setText("Schließen");
            QObject::connect(close,&QDialogButtonBox::rejected,photo,&QDialog::reject);
            layout->addWidget(close);
            const QScreen *screen=parent->screen();
            const QSize available=screen?screen->availableGeometry().size():QSize(1000,900);
            photo->resize(std::min(600,available.width()*4/5),std::min(820,available.height()*4/5));
            photo->show();
        });
    return label;
}
