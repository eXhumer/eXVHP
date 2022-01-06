#ifndef STREAMABLE_SERVICE_HXX
#define STREAMABLE_SERVICE_HXX

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

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
