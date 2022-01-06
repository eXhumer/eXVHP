#ifndef JUSTSTREAMLIVE_SERVICE_HXX
#define JUSTSTREAMLIVE_SERVICE_HXX

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>

namespace eXVHP::Service {
class JustStreamLive : public QObject {
  Q_OBJECT

private:
  QNetworkAccessManager *m_nam;
  static QString apiUrl;
  static QString baseUrl;

public:
  JustStreamLive(QNetworkAccessManager *nam, QObject *parent = nullptr);

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

#endif // JUSTSTREAMLIVE_SERVICE_HXX
