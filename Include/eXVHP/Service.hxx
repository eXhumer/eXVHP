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

#ifndef EXVHP_SERVICE_HXX
#define EXVHP_SERVICE_HXX

#include <QFile>
#include <QNetworkAccessManager>
#include <QRegularExpression>

namespace eXVHP::Service {
class MediaService : public QObject {
  Q_OBJECT

private:
  QNetworkAccessManager *m_nam;
  static QString jslApiUrl;
  static QString jslBaseUrl;
  static QRegularExpression sgglinkIdRegex;
  static QString sggParseLinkId(QString homePageData);
  static QString sabApiUrl;
  static QString sabAwsUrl;
  static QString sabBaseUrl;
  static QString sabReactVersion;
  static QString sffBaseUrl;
  static QString sjaBaseUrl;

public:
  MediaService(QNetworkAccessManager *nam = nullptr, QObject *parent = nullptr);

public slots:
  void uploadJustStreamLive(QFile *videoFile);
  void uploadStreamable(QFile *videoFile, const QString &videoTitle,
                        const QString &awsRegion);
  void uploadStreamff(QFile *videoFile);
  void uploadStreamja(QFile *videoFile);

signals:
  void mediaUploaded(QFile *videoFile, const QString &videoId,
                     const QString &videoLink);
  void mediaUploadError(QFile *videoFile, const QString &error);
  void mediaUploadProgress(QFile *videoFile, qint64 bytesSent,
                           qint64 bytesTotal);
};
} // namespace eXVHP::Service

#endif // EXVHP_SERVICE_HXX
