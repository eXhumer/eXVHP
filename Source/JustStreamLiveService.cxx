#include "JustStreamLive.hxx"
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkReply>

namespace eXVHP::Service {
QString JustStreamLive::apiUrl = "https://api.juststream.live";
QString JustStreamLive::baseUrl = "https://juststream.live";

JustStreamLive::JustStreamLive(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam) {}

void JustStreamLive::uploadVideo(QFile *videoFile) {
  QString videoFileName = QFileInfo(*videoFile).fileName();
  QString videoMimeType = QMimeDatabase().mimeTypeForFile(videoFileName).name();

  if (!QStringList{"video/mp4", "video/webm", "video/x-matroska"}.contains(
          videoMimeType)) {
    emit this->videoUploadError(
        videoFile,
        "Unsupported file type! JustStreamLive accepts MKV/MP4/WEBM!");
    return;
  }

  if (videoFile->size() > 200 * 0x100000) {
    emit this->videoUploadError(
        videoFile, "File too big! JustStreamLive supports 200MB maximum!");
    return;
  }

  QHttpMultiPart *uploadMultiPart =
      new QHttpMultiPart(QHttpMultiPart::FormDataType);

  QHttpPart videoFilePart;
  videoFilePart.setHeader(QNetworkRequest::ContentTypeHeader,
                          QVariant(videoMimeType));
  videoFilePart.setHeader(
      QNetworkRequest::ContentDispositionHeader,
      QVariant("form-data; name=\"file\"; filename=\"" + videoFileName + "\""));
  videoFile->open(QIODevice::ReadOnly);
  videoFilePart.setBodyDevice(videoFile);
  videoFile->setParent(uploadMultiPart);
  uploadMultiPart->append(videoFilePart);

  QNetworkReply *resp = m_nam->post(
      QNetworkRequest(QUrl(apiUrl + "/videos/upload")), uploadMultiPart);

  connect(resp, &QNetworkReply::uploadProgress, this,
          [this, videoFile](qint64 bytesSent, qint64 bytesTotal) {
            emit this->videoUploadProgress(videoFile, bytesSent, bytesTotal);
          });
  connect(resp, &QNetworkReply::finished, this, [this, resp, videoFile]() {
    if (resp->error() != QNetworkReply::NoError) {
      emit this->videoUploadError(videoFile, resp->errorString());
      return;
    }

    QString videoId =
        QJsonDocument::fromJson(resp->readAll()).object()["id"].toString();
    emit this->videoUploaded(videoFile, videoId,
                             JustStreamLive::baseUrl + "/" + videoId);
  });
  connect(resp, &QNetworkReply::finished, uploadMultiPart,
          &QHttpMultiPart::deleteLater);
  connect(resp, &QNetworkReply::finished, resp, &QNetworkReply::deleteLater);
}
} // namespace eXVHP::Service
