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

#ifndef STREAMABLE_SERVICE_HXX
#define STREAMABLE_SERVICE_HXX

#include <QFile>
#include <QNetworkAccessManager>

namespace eXVHP::Service {
class Streamable : public QObject {
  Q_OBJECT

private:
  QNetworkAccessManager *m_nam;
  static QString apiUrl;
  static QString baseUrl;
  static QString reactVersion;

public:
  Streamable(QNetworkAccessManager *nam, QObject *parent = nullptr);

public slots:
  void uploadVideo(QFile *videoFile, const QString &videoTitle,
                   const QString &awsRegion);

signals:
  void videoUploaded(QFile *videoFile, const QString &shortCode,
                     const QString &videoLink);
  void videoUploadError(QFile *videoFile, const QString &error);
  void videoUploadProgress(QFile *videoFile, qint64 bytesSent,
                           qint64 bytesTotal);
};
} // namespace eXVHP::Service

#endif // STREAMABLE_SERVICE_HXX
