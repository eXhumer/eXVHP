#ifndef STREAMFF_SERVICE_HXX
#define STREAMFF_SERVICE_HXX

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace eXVHP::Service {
class Streamff : public QObject {
  Q_OBJECT

private:
  QNetworkAccessManager *m_nam;
  static QString baseUrl;

public:
  Streamff(QNetworkAccessManager *nam, QObject *parent = nullptr);

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

#endif // STREAMFF_SERVICE_HXX
