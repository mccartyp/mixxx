#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <memory>

class ControlProxy;
class PlayerInfo;
class Track;

namespace mixxx {

class RestApiServer : public QObject {
    Q_OBJECT

  public:
    explicit RestApiServer(QObject* parent = nullptr);
    ~RestApiServer() override;

    bool start(quint16 port = 8080);
    void stop();
    bool isRunning() const;
    quint16 port() const;

  private slots:
    void handleNewConnection();
    void handleClientData();
    void handleClientDisconnected();

  private:
    struct HttpRequest {
        QString method;
        QString path;
        QMap<QString, QString> headers;
        QByteArray body;
    };

    struct HttpResponse {
        int statusCode = 200;
        QString statusText = "OK";
        QMap<QString, QString> headers;
        QByteArray body;
    };

    HttpRequest parseHttpRequest(const QByteArray& data);
    void sendHttpResponse(QTcpSocket* socket, const HttpResponse& response);
    void handleRequest(QTcpSocket* socket, const HttpRequest& request);

    // API endpoint handlers
    void handleGetStatus(QTcpSocket* socket, const HttpRequest& request);
    void handleGetPlayer(QTcpSocket* socket, const QString& group);
    void handleGetControl(QTcpSocket* socket, const QString& group, const QString& item);
    void handleSetControl(QTcpSocket* socket, const QString& group, const QString& item, const QByteArray& body);

    // Helper methods
    QJsonObject getPlayerStatus(const QString& group);
    QJsonObject getTrackMetadata(Track* track);
    QJsonObject getAllPlayersStatus();

    QTcpServer* m_pServer;
    quint16 m_port;
};

} // namespace mixxx
