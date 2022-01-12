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

#ifndef MIXTURE_SERVICE_HXX
#define MIXTURE_SERVICE_HXX

#include <QFile>
#include <QNetworkAccessManager>
#include <QRegularExpression>

namespace eXVHP::Service {
class Mixture : public QObject {
  Q_OBJECT

private:
  QNetworkAccessManager *m_nam;
  static QString baseUrl;
  static QRegularExpression linkIdRegex;
  static QString parseLinkId(QString homePageData);

public:
  Mixture(QNetworkAccessManager *nam, QObject *parent = nullptr);

public slots:
  void uploadVideo(QFile *videoFile);

signals:
  void videoUploaded(QFile *videoFile, const QString &linkId,
                     const QString &videoLink);
  void videoUploadError(QFile *videoFile, const QString &error);
  void videoUploadProgress(QFile *videoFile, qint64 bytesSent,
                           qint64 bytesTotal);
};
} // namespace eXVHP::Service

#endif // MIXTURE_SERVICE_HXX
