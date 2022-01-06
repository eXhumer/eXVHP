#include "Streamwo.hxx"
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRegularExpressionMatch>

namespace eXVHP::Service {
QString Streamwo::baseUrl = "https://streamwo.com";
QRegularExpression
    Streamwo::linkIdRegex("<input type=\"hidden\" name=\"link_id\" "
                          "id=\"link_id\" value=\"(?<linkId>[^\"]*)\" />");
QRegularExpression Streamwo::videoLinkRegex(
    "<source src=\"(?<videoLink>[^\"]*)\" type=\"video/mp4\">");

Streamwo::Streamwo(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam) {}

QString Streamwo::parseLinkId(QString homePageData) {
  QRegularExpressionMatch linkIdRegexMatch = linkIdRegex.match(homePageData);

  return linkIdRegexMatch.hasMatch() ? linkIdRegexMatch.captured("linkId")
                                     : QString();
}

void Streamwo::uploadVideo(QFile *videoFile) {
  QFileInfo videoFileInfo(*videoFile);
  QMimeDatabase mimeDb;
  QString videoMimeType =
      mimeDb.mimeTypeForFile(videoFileInfo.fileName()).name();

  if (videoMimeType != "video/mp4") {
    emit this->videoUploadError(
        videoFile, "Unsupported file type! Streamwo only accepts MP4!");
    return;
  }

  if (videoFile->size() > 512 * 0x100000) {
    emit this->videoUploadError(
        videoFile, "File too big! Streamwo supports 512MB maximum!");
    return;
  }

  QNetworkReply *homePageResp = m_nam->get(QNetworkRequest(QUrl(baseUrl)));

  connect(
      homePageResp, &QNetworkReply::finished, this,
      [this, homePageResp, videoFile, &videoFileInfo, videoMimeType]() {
        if (homePageResp->error() != QNetworkReply::NoError) {
          emit this->videoUploadError(videoFile, homePageResp->errorString());
          return;
        }

        QString linkId = parseLinkId(QString(homePageResp->readAll()));

        QHttpMultiPart *uploadMultiPart =
            new QHttpMultiPart(QHttpMultiPart::FormDataType);

        QHttpPart videoFilePart;
        videoFilePart.setHeader(QNetworkRequest::ContentTypeHeader,
                                QVariant(videoMimeType));
        videoFilePart.setHeader(
            QNetworkRequest::ContentDispositionHeader,
            QVariant("form-data; name=\"upload_file\"; filename=\"" +
                     videoFileInfo.fileName() + "\""));
        videoFile->open(QIODevice::ReadOnly);
        videoFilePart.setBodyDevice(videoFile);
        videoFile->setParent(uploadMultiPart);
        uploadMultiPart->append(videoFilePart);

        QHttpPart linkIdPart;
        linkIdPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                             QVariant("form-data; name=\"link_id\""));
        linkIdPart.setBody(linkId.toUtf8());
        uploadMultiPart->append(linkIdPart);

        QNetworkReply *uploadResp = this->m_nam->post(
            QNetworkRequest(QUrl(baseUrl + "/upload_file.php")),
            uploadMultiPart);

        connect(uploadResp, &QNetworkReply::uploadProgress, this,
                [this, videoFile](qint64 bytesSent, qint64 bytesTotal) {
                  emit this->videoUploadProgress(videoFile, bytesSent,
                                                 bytesTotal);
                });

        connect(uploadResp, &QNetworkReply::finished, this,
                [this, linkId, uploadResp, videoFile]() {
                  if (uploadResp->error() != QNetworkReply::NoError) {
                    emit this->videoUploadError(videoFile,
                                                uploadResp->errorString());
                    return;
                  }

                  emit this->videoUploaded(
                      videoFile, linkId, Streamwo::baseUrl + "/file/" + linkId);
                });

        connect(uploadResp, &QNetworkReply::finished, uploadMultiPart,
                &QHttpMultiPart::deleteLater);
        connect(uploadResp, &QNetworkReply::finished, uploadResp,
                &QNetworkReply::deleteLater);
      });

  connect(homePageResp, &QNetworkReply::finished, homePageResp,
          &QNetworkReply::deleteLater);
}
} // namespace eXVHP::Service
