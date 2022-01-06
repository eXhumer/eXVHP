#ifndef MIXTURE_SERVICE_HXX
#define MIXTURE_SERVICE_HXX

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>

namespace eXVHP::Service {
class Mixture : public QObject {
  Q_OBJECT

private:
  QNetworkAccessManager *m_nam;
  static QString baseUrl;
  static QRegularExpression linkIdRegex;
  static QRegularExpression videoLinkRegex;
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
