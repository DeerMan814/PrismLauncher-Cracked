#include "ImgurAlbumCreation.h"

#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QStringList>
#include <QDebug>

#include "BuildConfig.h"
#include "Application.h"

ImgurAlbumCreation::ImgurAlbumCreation(QList<ScreenShot::Ptr> screenshots) : NetAction(), m_screenshots(screenshots)
{
    m_url = BuildConfig.IMGUR_BASE_URL + "album.json";
    m_state = State::Inactive;
}

void ImgurAlbumCreation::executeTask()
{
    m_state = State::Running;
    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT_UNCACHED);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("Authorization", QString("Client-ID %1").arg(BuildConfig.IMGUR_CLIENT_ID).toStdString().c_str());
    request.setRawHeader("Accept", "application/json");

    QStringList hashes;
    for (auto shot : m_screenshots)
    {
        hashes.append(shot->m_imgurDeleteHash);
    }

    const QByteArray data = "deletehashes=" + hashes.join(',').toUtf8() + "&title=Minecraft%20Screenshots&privacy=hidden";

    QNetworkReply *rep = APPLICATION->network()->post(request, data);

    m_reply.reset(rep);
    connect(rep, &QNetworkReply::uploadProgress, this, &ImgurAlbumCreation::downloadProgress);
    connect(rep, &QNetworkReply::finished, this, &ImgurAlbumCreation::downloadFinished);
    connect(rep, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(downloadError(QNetworkReply::NetworkError)));
}
void ImgurAlbumCreation::downloadError(QNetworkReply::NetworkError error)
{
    qDebug() << m_reply->errorString();
    m_state = State::Failed;
}
void ImgurAlbumCreation::downloadFinished()
{
    if (m_state != State::Failed)
    {
        QByteArray data = m_reply->readAll();
        m_reply.reset();
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
        if (jsonError.error != QJsonParseError::NoError)
        {
            qDebug() << jsonError.errorString();
            emitFailed();
            return;
        }
        auto object = doc.object();
        if (!object.value("success").toBool())
        {
            qDebug() << doc.toJson();
            emitFailed();
            return;
        }
        m_deleteHash = object.value("data").toObject().value("deletehash").toString();
        m_id = object.value("data").toObject().value("id").toString();
        m_state = State::Succeeded;
        emit succeeded();
        return;
    }
    else
    {
        qDebug() << m_reply->readAll();
        m_reply.reset();
        emitFailed();
        return;
    }
}
void ImgurAlbumCreation::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    setProgress(bytesReceived, bytesTotal);
    emit progress(bytesReceived, bytesTotal);
}
