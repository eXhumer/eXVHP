#include "Streamff.hxx"
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QMimeDatabase>
#include <QNetworkReply>

namespace eXVHP::Service {
QString Streamff::baseUrl = "https://streamff.com";

Streamff::Streamff(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam) {}

void Streamff::uploadVideo(QFile *videoFile) {
  QFileInfo videoFileInfo(*videoFile);
  QMimeDatabase mimeDb;
  QString videoMimeType =
      mimeDb.mimeTypeForFile(videoFileInfo.fileName()).name();

  if (videoMimeType != "video/mp4") {
    emit this->videoUploadError(
        videoFile, "Unsupported file type! Streamff accepts MP4 only!");
    return;
  }

  if (videoFile->size() > 200 * 0x100000) {
    emit this->videoUploadError(
        videoFile, "File too big! Streamff supports 200MB maximum!");
    return;
  }

  QNetworkReply *generateResp =
      m_nam->post(QNetworkRequest(QUrl(baseUrl + "/api/videos/generate-link")),
                  QByteArray());
  QNetworkAccessManager *nam = m_nam;

  connect(
      generateResp, &QNetworkReply::finished, this,
      [this, generateResp, nam, videoFile, videoFileInfo, videoMimeType]() {
        if (generateResp->error() != QNetworkReply::NoError) {
          emit this->videoUploadError(videoFile, generateResp->errorString());
          return;
        }

        QString videoId(generateResp->readAll());
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

        QNetworkReply *uploadResp = nam->post(
            QNetworkRequest(QUrl(baseUrl + "/api/videos/upload/" + videoId)),
            uploadMultiPart);
        connect(uploadResp, &QNetworkReply::uploadProgress, this,
                [this, videoFile](qint64 bytesSent, qint64 bytesTotal) {
                  emit this->videoUploadProgress(videoFile, bytesSent,
                                                 bytesTotal);
                });
        connect(uploadResp, &QNetworkReply::finished, this,
                [this, videoId, uploadResp, videoFile]() {
                  if (uploadResp->error() != QNetworkReply::NoError) {
                    emit this->videoUploadError(videoFile,
                                                uploadResp->errorString());
                    return;
                  }

                  emit this->videoUploaded(videoFile, videoId,
                                           Streamff::baseUrl + "/v/" + videoId);
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
