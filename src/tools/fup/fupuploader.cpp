// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "fupuploader.h"
#include "src/utils/confighandler.h"
#include "src/utils/filenamehandler.h"
#include "src/utils/history.h"
#include "src/utils/systemnotification.h"
#include "src/widgets/imagelabel.h"
#include "src/widgets/loadspinner.h"
#include "src/widgets/notificationwidget.h"
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDrag>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRect>
#include <QScreen>
#include <QShortcut>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace {
QString UploadConfig::error()
{
    if (host == "" && key == "") {
        return "fup upload_host and upload_key both missing.";
    } else if (host == "") {
        return "fup upload_host missing.";
    } else if (key == "") {
        return "fup upload_key missing.";
    } else {
        return "";
    }
}
}

FupUploader::FupUploader(const QPixmap& capture, QWidget* parent)
  : QWidget(parent)
  , m_pixmap(capture)
{
    setWindowTitle(tr("Upload to Fup"));
    setWindowIcon(QIcon(":img/app/flameshot.svg"));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    QRect position = frameGeometry();
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    position.moveCenter(screen->availableGeometry().center());
    move(position.topLeft());
#endif

    m_spinner = new LoadSpinner(this);
    m_spinner->setColor(ConfigHandler().uiMainColorValue());
    m_spinner->start();

    m_infoLabel = new QLabel(tr("Uploading Image"));

    m_vLayout = new QVBoxLayout();
    setLayout(m_vLayout);
    m_vLayout->addWidget(m_spinner, 0, Qt::AlignHCenter);
    m_vLayout->addWidget(m_infoLabel);

    m_NetworkAM = new QNetworkAccessManager(this);
    connect(m_NetworkAM,
            &QNetworkAccessManager::finished,
            this,
            &FupUploader::handleReply);

    setAttribute(Qt::WA_DeleteOnClose);

    upload();
}

void FupUploader::handleReply(QNetworkReply* reply)
{
    m_spinner->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
        QJsonObject json = response.object();
        m_imageURL.setUrl(json[QStringLiteral("display_url")].toString());

        // save history
        QString imageName = m_imageURL.toString();
        int lastSlash = imageName.lastIndexOf("/");
        if (lastSlash >= 0) {
            imageName = imageName.mid(lastSlash + 1);
        }

        // save image to history
        History history;
        imageName = history.packFileName("fup", "", imageName);
        history.save(m_pixmap, imageName);

        if (ConfigHandler().copyAndCloseAfterUploadEnabled()) {
            this->copyURL(true);
            close();
        } else {
            onUploadOk();
        }
    } else {
        m_infoLabel->setText(reply->errorString());
    }
    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void FupUploader::startDrag()
{
    QMimeData* mimeData = new QMimeData;
    mimeData->setUrls(QList<QUrl>{ m_imageURL });
    mimeData->setImageData(m_pixmap);

    QDrag* dragHandler = new QDrag(this);
    dragHandler->setMimeData(mimeData);
    dragHandler->setPixmap(m_pixmap.scaled(
      256, 256, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    dragHandler->exec();
}

UploadConfig FupUploader::config()
{
    return UploadConfig{
        .host = ConfigHandler().value("fup", "upload_host").toString(),
        .key = ConfigHandler().value("fup", "upload_key").toString(),
    };
}

void FupUploader::upload()
{
    UploadConfig config = this->config();
    if (config.error() != "") {
        m_spinner->deleteLater();
        m_infoLabel->setText(config.error());
        new QShortcut(Qt::Key_Escape, this, SLOT(close()));
        return;
    }

    QString basicAuthValue = QStringLiteral(":%1").arg(config.key).toUtf8().toBase64();

    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    m_pixmap.save(&buffer, "PNG");

    QUrl url(QStringLiteral("https://%1/upload/image.png").arg(config.host).toUtf8());
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "image/png");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader(
      "Authorization",
      QStringLiteral("Basic %1").arg(basicAuthValue).toUtf8());

    m_NetworkAM->put(request, byteArray);
}

void FupUploader::onUploadOk()
{
    m_infoLabel->deleteLater();

    m_notification = new NotificationWidget();
    m_vLayout->addWidget(m_notification);

    ImageLabel* imageLabel = new ImageLabel();
    imageLabel->setScreenshot(m_pixmap);
    imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(
      imageLabel, &ImageLabel::dragInitiated, this, &FupUploader::startDrag);
    m_vLayout->addWidget(imageLabel);

    m_hLayout = new QHBoxLayout();
    m_vLayout->addLayout(m_hLayout);

    m_copyUrlButton = new QPushButton(tr("Copy URL"));
    m_openUrlButton = new QPushButton(tr("Open URL"));
    m_toClipboardButton = new QPushButton(tr("Image to Clipboard."));
    m_hLayout->addWidget(m_copyUrlButton);
    m_hLayout->addWidget(m_openUrlButton);
    m_hLayout->addWidget(m_toClipboardButton);

    connect(
      m_copyUrlButton, &QPushButton::clicked, this, &FupUploader::copyURL);
    connect(
      m_openUrlButton, &QPushButton::clicked, this, &FupUploader::openURL);
    connect(m_toClipboardButton,
            &QPushButton::clicked,
            this,
            &FupUploader::copyImage);
}

void FupUploader::openURL()
{
    bool successful = QDesktopServices::openUrl(m_imageURL);
    if (!successful) {
        m_notification->showMessage(tr("Unable to open the URL."));
    }
}

void FupUploader::copyURL(bool useSystemNotification)
{
    auto* clipboard = QApplication::clipboard();
    clipboard->setText(m_imageURL.toString(), QClipboard::Clipboard);
    if (clipboard->supportsSelection()) {
        clipboard->setText(m_imageURL.toString(), QClipboard::Selection);
    }

#if defined(Q_OS_LINUX)
    QThread::msleep(1); //workaround for copied text not being available...
#endif

    if (useSystemNotification) {
        SystemNotification().sendMessage(
            QObject::tr("URL copied to clipboard."));
    } else {
        m_notification->showMessage(tr("URL copied to clipboard."));
    }
}

void FupUploader::copyImage()
{
    QApplication::clipboard()->setPixmap(m_pixmap);
    m_notification->showMessage(tr("Screenshot copied to clipboard."));
}
