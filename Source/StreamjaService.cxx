#include "Streamja.hxx"
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QUrlQuery>

namespace eXVHP::Service {
QString Streamja::baseUrl = "https://streamja.com";

Streamja::Streamja(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam) {}

void Streamja::uploadVideo(QFile *videoFile) {
  QFileInfo videoFileInfo(*videoFile);
  QMimeDatabase mimeDb;
  QString videoMimeType =
      mimeDb.mimeTypeForFile(videoFileInfo.fileName()).name();

  if (videoMimeType != "video/mp4") {
    emit this->videoUploadError(
        videoFile, "Unsupported file type! Streamja only accepts MP4!");
    return;
  }

  if (videoFile->size() > 30 * 0x100000) {
    emit this->videoUploadError(
        videoFile, "File too big! Streamja supports 30MB maximum!");
    return;
  }

  QNetworkAccessManager *nam = m_nam;

  QNetworkRequest generateReq(QUrl(baseUrl + "/shortId.php"));
  generateReq.setHeader(QNetworkRequest::ContentTypeHeader,
                        "application/x-www-form-urlencoded");
  QNetworkReply *generateResp =
      nam->post(generateReq,
                QUrlQuery{{"new", "1"}}.toString(QUrl::FullyEncoded).toUtf8());

  connect(generateResp, &QNetworkReply::finished, this,
          [this, generateResp, nam, videoFile, videoFileInfo, videoMimeType]() {
            if (generateResp->error() != QNetworkReply::NoError) {
              emit this->videoUploadError(videoFile,
                                          generateResp->errorString());
              return;
            }

            QString shortId = QJsonDocument::fromJson(generateResp->readAll())
                                  .object()["shortId"]
                                  .toString();

            QHttpMultiPart *uploadMultiPart =
                new QHttpMultiPart(QHttpMultiPart::FormDataType);

            QHttpPart videoFilePart;
            videoFilePart.setHeader(QNetworkRequest::ContentTypeHeader,
                                    QVariant(videoMimeType));
            videoFilePart.setHeader(
                QNetworkRequest::ContentDispositionHeader,
                QVariant("form-data; name=\"file\"; filename=\"" +
                         videoFileInfo.fileName() + "\""));
            videoFile->open(QIODevice::ReadOnly);
            videoFilePart.setBodyDevice(videoFile);
            videoFile->setParent(uploadMultiPart);
            uploadMultiPart->append(videoFilePart);

            QUrl uploadUrl(baseUrl + "/upload.php");
            QUrlQuery uploadQuery{{"shortId", shortId}};
            uploadUrl.setQuery(uploadQuery);

            QNetworkReply *uploadResp =
                nam->post(QNetworkRequest(uploadUrl), uploadMultiPart);
            connect(uploadResp, &QNetworkReply::uploadProgress, this,
                    [this, videoFile](qint64 bytesSent, qint64 bytesTotal) {
                      emit this->videoUploadProgress(videoFile, bytesSent,
                                                     bytesTotal);
                    });
            connect(uploadResp, &QNetworkReply::finished, this,
                    [this, shortId, uploadResp, videoFile]() {
                      if (uploadResp->error() != QNetworkReply::NoError) {
                        emit this->videoUploadError(videoFile,
                                                    uploadResp->errorString());
                        return;
                      }

                      emit this->videoUploaded(videoFile, shortId,
                                               baseUrl + "/" + shortId,
                                               baseUrl + "/embed/" + shortId);
                    });
            connect(uploadResp, &QNetworkReply::finished, uploadMultiPart,
                    &QHttpMultiPart::deleteLater);
            connect(uploadResp, &QNetworkReply::finished, uploadResp,
                    &QNetworkReply::deleteLater);
          });

  connect(generateResp, &QNetworkReply::finished, generateResp,
          &QNetworkReply::deleteLater);
}
} // namespace eXVHP::Service
