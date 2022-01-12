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

#include "Streamable.hxx"
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QUrlQuery>

namespace eXVHP::Service {
QString Streamable::apiUrl = "https://ajax.streamable.com";
QString Streamable::baseUrl = "https://streamable.com";
QString Streamable::reactVersion = "03db98af3545197e67cb96893d9e9d8729eee743";

Streamable::Streamable(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam) {}

void Streamable::uploadVideo(QFile *videoFile, const QString &videoTitle,
                             const QString &awsRegion) {
  QString videoFileName = QFileInfo(*videoFile).fileName();
  QString videoMimeType = QMimeDatabase().mimeTypeForFile(videoFileName).name();

  if (!QStringList{"video/mp4", "video/x-matroska"}.contains(videoMimeType)) {
    emit this->videoUploadError(
        videoFile, "Unsupported file type! Streamable only accepts MKV/MP4!");
    return;
  }

  if (videoFile->size() > 250 * 0x100000) {
    emit this->videoUploadError(
        videoFile, "File too big! Streamable supports 250MB maximum!");
    return;
  }

  QNetworkAccessManager *nam = m_nam;

  QUrl shortcodeUrl(apiUrl + "/shortcode");
  shortcodeUrl.setQuery(QUrlQuery{
      {"version", reactVersion}, {"size", QString::number(videoFile->size())}});
  QNetworkReply *generateResp = nam->get(QNetworkRequest(shortcodeUrl));
  connect(
      generateResp, &QNetworkReply::finished, this,
      [this, awsRegion, generateResp, nam, videoFile, videoFileName,
       videoTitle]() {
        if (generateResp->error() != QNetworkReply::NoError) {
          emit this->videoUploadError(videoFile, generateResp->errorString());
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

        QUrl updateMetaUrl(apiUrl + "/videos/" + shortCode);
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
        QNetworkReply *updateMetaResp = nam->put(
            updateMetaReq,
            QJsonDocument(videoMetaJson).toJson(QJsonDocument::Compact));
        connect(
            updateMetaResp, &QNetworkReply::finished, this,
            [this, accessKeyId, awsRegion, nam, secretAccessKey, sessionToken,
             shortCode, transcoderToken, updateMetaResp, videoFile]() {
              if (updateMetaResp->error() != QNetworkReply::NoError) {
                emit this->videoUploadError(videoFile,
                                            updateMetaResp->errorString());
                return;
              }

              QNetworkRequest uploadReq(
                  QUrl("https://streamables-upload.s3.amazonaws.com/upload/" +
                       shortCode));
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
              QNetworkReply *uploadResp = nam->put(uploadReq, videoFile);

              connect(uploadResp, &QNetworkReply::uploadProgress, this,
                      [this, videoFile](qint64 bytesSent, qint64 bytesTotal) {
                        emit this->videoUploadProgress(videoFile, bytesSent,
                                                       bytesTotal);
                      });

              connect(
                  uploadResp, &QNetworkReply::finished, this,
                  [this, nam, shortCode, transcoderToken, uploadResp,
                   videoFile]() {
                    if (uploadResp->error() != QNetworkReply::NoError) {
                      emit this->videoUploadError(videoFile,
                                                  uploadResp->errorString());
                      return;
                    }

                    QNetworkRequest transcodeReq(
                        QUrl(apiUrl + "/transcode/" + shortCode));
                    transcodeReq.setHeader(QNetworkRequest::ContentTypeHeader,
                                           "application/json");

                    QNetworkReply *transcodeResp = nam->post(
                        transcodeReq,
                        QJsonDocument(
                            QJsonObject{
                                {"shortcode", shortCode},
                                {"size", videoFile->size()},
                                {"token", transcoderToken},
                                {"upload_source", "web"},
                                {"url",
                                 "https://streamables-upload.s3.amazonaws.com/"
                                 "upload/" +
                                     shortCode}})
                            .toJson(QJsonDocument::Compact));

                    connect(transcodeResp, &QNetworkReply::finished, this,
                            [this, shortCode, transcodeResp, videoFile]() {
                              if (transcodeResp->error() !=
                                  QNetworkReply::NoError) {
                                emit this->videoUploadError(
                                    videoFile, transcodeResp->errorString());
                                return;
                              }

                              emit this->videoUploaded(videoFile, shortCode,
                                                       baseUrl + "/" +
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
} // namespace eXVHP::Service
