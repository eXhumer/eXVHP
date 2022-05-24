/*
 * eXVHP - External video hosting platform communication layer via Qt
 * Copyright (C) 2021 - eXhumer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Service.hxx"
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QUrlQuery>

namespace eXVHP::Service {
QString MediaService::jslApiUrl = "https://api.juststream.live";
QString MediaService::jslBaseUrl = "https://juststream.live";
QString MediaService::sabApiUrl = "https://ajax.streamable.com";
QString MediaService::sabAwsUrl = "https://streamables-upload.s3.amazonaws.com";
QString MediaService::sabBaseUrl = "https://streamable.com";
QString MediaService::sabReactVersion =
    "03db98af3545197e67cb96893d9e9d8729eee743";
QString MediaService::sffBaseUrl = "https://streamff.com";
QString MediaService::sjaBaseUrl = "https://streamja.com";
QRegularExpression MediaService::sgglinkIdRegex(
    "<input type=\"hidden\" name=\"link_id\" "
    "id=\"link_id\" value=\"(?<linkId>[^\"]*)\" />");

QString MediaService::sggParseLinkId(QString homePageData) {
  QRegularExpressionMatch linkIdRegexMatch = sgglinkIdRegex.match(homePageData);

  return linkIdRegexMatch.hasMatch() ? linkIdRegexMatch.captured("linkId")
                                     : QString();
}

MediaService::MediaService(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent) {
  if (nam == nullptr)
    nam = new QNetworkAccessManager(this);

  m_nam = nam;
}

void MediaService::uploadJustStreamLive(QFile *videoFile) {
  QString videoFileName = QFileInfo(*videoFile).fileName();
  QString videoMimeType = QMimeDatabase().mimeTypeForFile(videoFileName).name();

  if (!QStringList{"video/mp4", "video/webm", "video/x-matroska"}.contains(
          videoMimeType)) {
    emit this->mediaUploadError(
        videoFile,
        "Unsupported file type! JustStreamLive accepts MKV/MP4/WEBM!");
    return;
  }

  if (videoFile->size() > 200 * 0x100000) {
    emit this->mediaUploadError(
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
      QNetworkRequest(QUrl(jslApiUrl + "/videos/upload")), uploadMultiPart);

  connect(resp, &QNetworkReply::uploadProgress, this,
          [this, videoFile](qint64 bytesSent, qint64 bytesTotal) {
            emit this->mediaUploadProgress(videoFile, bytesSent, bytesTotal);
          });
  connect(resp, &QNetworkReply::finished, this, [this, resp, videoFile]() {
    if (resp->error() != QNetworkReply::NoError) {
      emit this->mediaUploadError(videoFile, resp->errorString());
      return;
    }

    QString videoId =
        QJsonDocument::fromJson(resp->readAll()).object()["id"].toString();
    emit this->mediaUploaded(videoFile, videoId, jslBaseUrl + "/" + videoId);
  });
  connect(resp, &QNetworkReply::finished, uploadMultiPart,
          &QHttpMultiPart::deleteLater);
  connect(resp, &QNetworkReply::finished, resp, &QNetworkReply::deleteLater);
}

void MediaService::uploadStreamable(QFile *videoFile, const QString &videoTitle,
                                    const QString &awsRegion) {
  QString videoFileName = QFileInfo(*videoFile).fileName();
  QString videoMimeType = QMimeDatabase().mimeTypeForFile(videoFileName).name();

  if (!QStringList{"video/mp4", "video/x-matroska"}.contains(videoMimeType)) {
    emit this->mediaUploadError(
        videoFile, "Unsupported file type! Streamable only accepts MKV/MP4!");
    return;
  }

  if (videoFile->size() > 250 * 0x100000) {
    emit this->mediaUploadError(
        videoFile, "File too big! Streamable supports 250MB maximum!");
    return;
  }

  QUrl shortcodeUrl(sabApiUrl + "/shortcode");
  shortcodeUrl.setQuery(
      QUrlQuery{{"version", sabReactVersion},
                {"size", QString::number(videoFile->size())}});
  QNetworkReply *generateResp = m_nam->get(QNetworkRequest(shortcodeUrl));
  connect(
      generateResp, &QNetworkReply::finished, this,
      [this, awsRegion, generateResp, videoFile, videoFileName, videoTitle]() {
        if (generateResp->error() != QNetworkReply::NoError) {
          emit this->mediaUploadError(videoFile, generateResp->errorString());
          return;
        }

        QJsonObject generateJson =
            QJsonDocument::fromJson(generateResp->readAll()).object();
        QString shortCode = generateJson["shortcode"].toString();
        QString accessKeyId =
            generateJson["credentials"].toObject()["accessKeyId"].toString();
        QString secretAccessKey = generateJson["credentials"]
                                      .toObject()["secretAccessKey"]
                                      .toString();
        QString sessionToken =
            generateJson["credentials"].toObject()["sessionToken"].toString();
        QString transcoderToken =
            generateJson["transcoder_options"].toObject()["token"].toString();

        QUrl updateMetaUrl(sabApiUrl + "/videos/" + shortCode);
        updateMetaUrl.setQuery(QUrlQuery{{"purge", ""}});
        QJsonObject videoMetaJson;
        videoMetaJson["original_name"] = videoFileName;
        videoMetaJson["original_size"] = videoFile->size();
        videoMetaJson["title"] = videoTitle.isEmpty()
                                     ? QFileInfo(*videoFile).baseName()
                                     : videoTitle;
        videoMetaJson["upload_source"] = "web";
        QNetworkRequest updateMetaReq(updateMetaUrl);
        updateMetaReq.setHeader(QNetworkRequest::ContentTypeHeader,
                                "application/json");
        QNetworkReply *updateMetaResp = m_nam->put(
            updateMetaReq,
            QJsonDocument(videoMetaJson).toJson(QJsonDocument::Compact));
        connect(
            updateMetaResp, &QNetworkReply::finished, this,
            [this, accessKeyId, awsRegion, secretAccessKey, sessionToken,
             shortCode, transcoderToken, updateMetaResp, videoFile]() {
              if (updateMetaResp->error() != QNetworkReply::NoError) {
                emit this->mediaUploadError(videoFile,
                                            updateMetaResp->errorString());
                return;
              }

              QNetworkRequest uploadReq(
                  QUrl(sabAwsUrl + "/upload/" + shortCode));
              uploadReq.setHeader(QNetworkRequest::ContentTypeHeader,
                                  "application/octet-stream");
              uploadReq.setRawHeader("x-amz-security-token",
                                     sessionToken.toUtf8());
              uploadReq.setRawHeader("x-amz-acl", "public-read");

              QCryptographicHash sha256Hash(QCryptographicHash::Sha256);
              videoFile->open(QFile::ReadOnly);
              sha256Hash.addData(videoFile);
              QString payloadHash = sha256Hash.result().toHex().toLower();
              uploadReq.setRawHeader("x-amz-content-sha256",
                                     payloadHash.toUtf8());

              QDateTime reqTime = QDateTime::currentDateTimeUtc();
              uploadReq.setRawHeader(
                  "x-amz-date", reqTime.toString("yyyyMMddTHHmmssZ").toUtf8());

              QMap<QString, QByteArray> canonicalHeaders;

              for (auto &&hKey : uploadReq.rawHeaderList()) {
                QString hKeyStr(hKey);

                if (hKeyStr.toLower() == "content-type" ||
                    hKeyStr.toLower() == "host" ||
                    hKeyStr.toLower().startsWith("x-amz-"))
                  canonicalHeaders[hKeyStr.toLower()] =
                      uploadReq.rawHeader(hKey);
              }

              if (!canonicalHeaders.contains("host"))
                canonicalHeaders["host"] =
                    "streamables-upload.s3.amazonaws.com";

              QString canonicalHeadersStr("");

              for (auto &&hKey : canonicalHeaders.keys())
                canonicalHeadersStr +=
                    hKey + ":" + QString(canonicalHeaders[hKey] + "\n");
              QString canonicalRequest =
                  QStringList{"PUT",
                              "/upload/" + shortCode,
                              "",
                              canonicalHeadersStr,
                              canonicalHeaders.keys().join(";"),
                              payloadHash}
                      .join("\n");

              sha256Hash.reset();
              sha256Hash.addData(canonicalRequest.toUtf8());
              QString scope =
                  QStringList{reqTime.date().toString("yyyyMMdd"),
                              awsRegion.isEmpty() ? "us-east-1" : awsRegion,
                              "s3", "aws4_request"}
                      .join("/");

              QString strToSign =
                  QStringList{"AWS4-HMAC-SHA256",
                              reqTime.toString("yyyyMMddTHHmmssZ"), scope,
                              sha256Hash.result().toHex().toLower()}
                      .join("\n");

              QMessageAuthenticationCode hmacSha256(QCryptographicHash::Sha256);
              hmacSha256.setKey(("AWS4" + secretAccessKey).toUtf8());
              hmacSha256.addData(reqTime.date().toString("yyyyMMdd").toUtf8());
              QByteArray dateKey = hmacSha256.result();
              hmacSha256.reset();
              hmacSha256.setKey(dateKey);
              hmacSha256.addData(awsRegion.toUtf8());
              QByteArray dateRegionKey = hmacSha256.result();
              hmacSha256.reset();
              hmacSha256.setKey(dateRegionKey);
              hmacSha256.addData("s3");
              QByteArray dateRegionServiceKey = hmacSha256.result();
              hmacSha256.reset();
              hmacSha256.setKey(dateRegionServiceKey);
              hmacSha256.addData("aws4_request");
              QByteArray signingKey = hmacSha256.result();
              hmacSha256.reset();
              hmacSha256.setKey(signingKey);
              hmacSha256.addData(strToSign.toUtf8());
              QString signature = hmacSha256.result().toHex().toLower();
              QString authorization =
                  "AWS4-HMAC-SHA256 Credential=" + accessKeyId + "/" + scope +
                  ",SignedHeaders=" + canonicalHeaders.keys().join(";") +
                  ",Signature=" + signature;

              uploadReq.setRawHeader("Authorization", authorization.toUtf8());
              videoFile->seek(0);
              QNetworkReply *uploadResp = m_nam->put(uploadReq, videoFile);

              connect(uploadResp, &QNetworkReply::uploadProgress, this,
                      [this, videoFile](qint64 bytesSent, qint64 bytesTotal) {
                        emit this->mediaUploadProgress(videoFile, bytesSent,
                                                       bytesTotal);
                      });

              connect(
                  uploadResp, &QNetworkReply::finished, this,
                  [this, shortCode, transcoderToken, uploadResp, videoFile]() {
                    if (uploadResp->error() != QNetworkReply::NoError) {
                      emit this->mediaUploadError(videoFile,
                                                  uploadResp->errorString());
                      return;
                    }

                    QNetworkRequest transcodeReq(
                        QUrl(sabApiUrl + "/transcode/" + shortCode));
                    transcodeReq.setHeader(QNetworkRequest::ContentTypeHeader,
                                           "application/json");

                    QNetworkReply *transcodeResp = m_nam->post(
                        transcodeReq,
                        QJsonDocument(
                            QJsonObject{
                                {"shortcode", shortCode},
                                {"size", videoFile->size()},
                                {"token", transcoderToken},
                                {"upload_source", "web"},
                                {"url", sabAwsUrl + "/upload/" + shortCode}})
                            .toJson(QJsonDocument::Compact));

                    connect(transcodeResp, &QNetworkReply::finished, this,
                            [this, shortCode, transcodeResp, videoFile]() {
                              if (transcodeResp->error() !=
                                  QNetworkReply::NoError) {
                                emit this->mediaUploadError(
                                    videoFile, transcodeResp->errorString());
                                return;
                              }

                              emit this->mediaUploaded(videoFile, shortCode,
                                                       sabBaseUrl + "/" +
                                                           shortCode);
                            });
                    connect(transcodeResp, &QNetworkReply::finished,
                            transcodeResp, &QNetworkReply::deleteLater);
                  });
              connect(uploadResp, &QNetworkReply::finished, uploadResp,
                      &QNetworkReply::deleteLater);
              connect(uploadResp, &QNetworkReply::finished, videoFile,
                      &QFile::deleteLater);
            });
        connect(updateMetaResp, &QNetworkReply::finished, updateMetaResp,
                &QNetworkReply::deleteLater);
      });
  connect(generateResp, &QNetworkReply::finished, generateResp,
          &QNetworkReply::deleteLater);
}

void MediaService::uploadStreamff(QFile *videoFile) {
  QString videoFileName = QFileInfo(*videoFile).fileName();
  QString videoMimeType = QMimeDatabase().mimeTypeForFile(videoFileName).name();

  if (videoMimeType != "video/mp4") {
    emit this->mediaUploadError(
        videoFile, "Unsupported file type! Streamff accepts MP4 only!");
    return;
  }

  if (videoFile->size() > 200 * 0x100000) {
    emit this->mediaUploadError(
        videoFile, "File too big! Streamff supports 200MB maximum!");
    return;
  }

  QNetworkReply *generateResp = m_nam->post(
      QNetworkRequest(QUrl(sffBaseUrl + "/api/videos/generate-link")),
      QByteArray());
  QNetworkAccessManager *nam = m_nam;

  connect(
      generateResp, &QNetworkReply::finished, this,
      [this, generateResp, nam, videoFile, videoFileName, videoMimeType]() {
        if (generateResp->error() != QNetworkReply::NoError) {
          emit this->mediaUploadError(videoFile, generateResp->errorString());
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
            QVariant("form-data; name=\"file\"; filename=\"" + videoFileName +
                     "\""));
        videoFile->open(QIODevice::ReadOnly);
        videoFilePart.setBodyDevice(videoFile);
        videoFile->setParent(uploadMultiPart);
        uploadMultiPart->append(videoFilePart);

        QNetworkReply *uploadResp = nam->post(
            QNetworkRequest(QUrl(sffBaseUrl + "/api/videos/upload/" + videoId)),
            uploadMultiPart);
        connect(uploadResp, &QNetworkReply::uploadProgress, this,
                [this, videoFile](qint64 bytesSent, qint64 bytesTotal) {
                  emit this->mediaUploadProgress(videoFile, bytesSent,
                                                 bytesTotal);
                });
        connect(uploadResp, &QNetworkReply::finished, this,
                [this, videoId, uploadResp, videoFile]() {
                  if (uploadResp->error() != QNetworkReply::NoError) {
                    emit this->mediaUploadError(videoFile,
                                                uploadResp->errorString());
                    return;
                  }

                  emit this->mediaUploaded(videoFile, videoId,
                                           sffBaseUrl + "/v/" + videoId);
                });
        connect(uploadResp, &QNetworkReply::finished, uploadMultiPart,
                &QHttpMultiPart::deleteLater);
        connect(uploadResp, &QNetworkReply::finished, uploadResp,
                &QNetworkReply::deleteLater);
      });
  connect(generateResp, &QNetworkReply::finished, generateResp,
          &QNetworkReply::deleteLater);
}

void MediaService::uploadStreamja(QFile *videoFile) {
  QString videoFileName = QFileInfo(*videoFile).fileName();
  QString videoMimeType = QMimeDatabase().mimeTypeForFile(videoFileName).name();

  if (videoMimeType != "video/mp4") {
    emit this->mediaUploadError(
        videoFile, "Unsupported file type! Streamja only accepts MP4!");
    return;
  }

  if (videoFile->size() > 30 * 0x100000) {
    emit this->mediaUploadError(
        videoFile, "File too big! Streamja supports 30MB maximum!");
    return;
  }

  QNetworkAccessManager *nam = m_nam;

  QNetworkRequest generateReq(QUrl(sjaBaseUrl + "/shortId.php"));
  generateReq.setHeader(QNetworkRequest::ContentTypeHeader,
                        "application/x-www-form-urlencoded");
  QNetworkReply *generateResp =
      nam->post(generateReq,
                QUrlQuery{{"new", "1"}}.toString(QUrl::FullyEncoded).toUtf8());

  connect(generateResp, &QNetworkReply::finished, this,
          [this, generateResp, nam, videoFile, videoFileName, videoMimeType]() {
            if (generateResp->error() != QNetworkReply::NoError) {
              emit this->mediaUploadError(videoFile,
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
                         videoFileName + "\""));
            videoFile->open(QIODevice::ReadOnly);
            videoFilePart.setBodyDevice(videoFile);
            videoFile->setParent(uploadMultiPart);
            uploadMultiPart->append(videoFilePart);

            QUrl uploadUrl(sjaBaseUrl + "/upload.php");
            QUrlQuery uploadQuery{{"shortId", shortId}};
            uploadUrl.setQuery(uploadQuery);

            QNetworkReply *uploadResp =
                nam->post(QNetworkRequest(uploadUrl), uploadMultiPart);
            connect(uploadResp, &QNetworkReply::uploadProgress, this,
                    [this, videoFile](qint64 bytesSent, qint64 bytesTotal) {
                      emit this->mediaUploadProgress(videoFile, bytesSent,
                                                     bytesTotal);
                    });
            connect(uploadResp, &QNetworkReply::finished, this,
                    [this, shortId, uploadResp, videoFile]() {
                      if (uploadResp->error() != QNetworkReply::NoError) {
                        emit this->mediaUploadError(videoFile,
                                                    uploadResp->errorString());
                        return;
                      }

                      emit this->mediaUploaded(videoFile, shortId,
                                               sjaBaseUrl + "/" + shortId);
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
