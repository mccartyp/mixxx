#include "api/restapiserver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUrl>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "mixer/playerinfo.h"
#include "track/track.h"
#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("RestApiServer");
const QString kDefaultGroup = "[Channel1]";
}

namespace mixxx {

RestApiServer::RestApiServer(QObject* parent)
        : QObject(parent),
          m_pServer(new QTcpServer(this)),
          m_port(0) {
    connect(m_pServer,
            &QTcpServer::newConnection,
            this,
            &RestApiServer::handleNewConnection);
}

RestApiServer::~RestApiServer() {
    stop();
}

bool RestApiServer::start(quint16 port) {
    if (m_pServer->isListening()) {
        kLogger.warning() << "REST API server already running on port" << m_port;
        return false;
    }

    if (!m_pServer->listen(QHostAddress::LocalHost, port)) {
        kLogger.warning() << "Failed to start REST API server on port" << port
                         << ":" << m_pServer->errorString();
        return false;
    }

    m_port = m_pServer->serverPort();
    kLogger.info() << "REST API server started on http://localhost:" << m_port;
    return true;
}

void RestApiServer::stop() {
    if (m_pServer->isListening()) {
        m_pServer->close();
        kLogger.info() << "REST API server stopped";
    }
}

bool RestApiServer::isRunning() const {
    return m_pServer->isListening();
}

quint16 RestApiServer::port() const {
    return m_port;
}

void RestApiServer::handleNewConnection() {
    QTcpSocket* socket = m_pServer->nextPendingConnection();
    if (!socket) {
        return;
    }

    connect(socket, &QTcpSocket::readyRead, this, &RestApiServer::handleClientData);
    connect(socket,
            &QTcpSocket::disconnected,
            this,
            &RestApiServer::handleClientDisconnected);

    kLogger.debug() << "New client connected:"
                   << socket->peerAddress().toString();
}

void RestApiServer::handleClientData() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    QByteArray data = socket->readAll();
    if (data.isEmpty()) {
        return;
    }

    HttpRequest request = parseHttpRequest(data);
    handleRequest(socket, request);
}

void RestApiServer::handleClientDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

RestApiServer::HttpRequest RestApiServer::parseHttpRequest(const QByteArray& data) {
    HttpRequest request;

    // Split request into lines
    QList<QByteArray> lines = data.split('\n');
    if (lines.isEmpty()) {
        return request;
    }

    // Parse request line (e.g., "GET /api/status HTTP/1.1")
    QList<QByteArray> requestLine = lines[0].trimmed().split(' ');
    if (requestLine.size() >= 2) {
        request.method = QString::fromUtf8(requestLine[0]);
        request.path = QString::fromUtf8(requestLine[1]);
    }

    // Parse headers
    int headerEndIndex = 1;
    for (int i = 1; i < lines.size(); ++i) {
        QByteArray line = lines[i].trimmed();
        if (line.isEmpty()) {
            headerEndIndex = i + 1;
            break;
        }

        int colonIndex = line.indexOf(':');
        if (colonIndex > 0) {
            QString key = QString::fromUtf8(line.left(colonIndex).trimmed());
            QString value = QString::fromUtf8(line.mid(colonIndex + 1).trimmed());
            request.headers[key.toLower()] = value;
        }
    }

    // Parse body (everything after empty line)
    if (headerEndIndex < lines.size()) {
        request.body = lines.mid(headerEndIndex).join('\n');
    }

    return request;
}

void RestApiServer::sendHttpResponse(QTcpSocket* socket, const HttpResponse& response) {
    if (!socket || !socket->isWritable()) {
        return;
    }

    QByteArray responseData;

    // Status line
    responseData.append("HTTP/1.1 ");
    responseData.append(QByteArray::number(response.statusCode));
    responseData.append(" ");
    responseData.append(response.statusText.toUtf8());
    responseData.append("\r\n");

    // Headers
    QMap<QString, QString> headers = response.headers;
    headers["Content-Length"] = QString::number(response.body.size());
    if (!headers.contains("Content-Type")) {
        headers["Content-Type"] = "application/json";
    }
    headers["Access-Control-Allow-Origin"] = "*";
    headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
    headers["Access-Control-Allow-Headers"] = "Content-Type";

    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        responseData.append(it.key().toUtf8());
        responseData.append(": ");
        responseData.append(it.value().toUtf8());
        responseData.append("\r\n");
    }

    responseData.append("\r\n");

    // Body
    responseData.append(response.body);

    socket->write(responseData);
    socket->flush();
    socket->disconnectFromHost();
}

void RestApiServer::handleRequest(QTcpSocket* socket, const HttpRequest& request) {
    kLogger.debug() << request.method << request.path;

    // Handle CORS preflight
    if (request.method == "OPTIONS") {
        HttpResponse response;
        response.statusCode = 204;
        response.statusText = "No Content";
        sendHttpResponse(socket, response);
        return;
    }

    // Route requests
    QStringList pathParts = request.path.split('/', Qt::SkipEmptyParts);

    // Remove query string if present
    if (!pathParts.isEmpty()) {
        QString& lastPart = pathParts.last();
        int queryIndex = lastPart.indexOf('?');
        if (queryIndex >= 0) {
            lastPart = lastPart.left(queryIndex);
        }
    }

    if (pathParts.isEmpty() || pathParts[0] != "api") {
        HttpResponse response;
        response.statusCode = 404;
        response.statusText = "Not Found";
        QJsonObject json;
        json["error"] = "Not Found";
        json["message"] = "API endpoints are under /api";
        response.body = QJsonDocument(json).toJson();
        sendHttpResponse(socket, response);
        return;
    }

    if (pathParts.size() < 2) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        QJsonObject json;
        json["error"] = "Bad Request";
        response.body = QJsonDocument(json).toJson();
        sendHttpResponse(socket, response);
        return;
    }

    QString endpoint = pathParts[1];

    if (endpoint == "status" && request.method == "GET") {
        handleGetStatus(socket, request);
    } else if (endpoint == "player" && pathParts.size() >= 3 && request.method == "GET") {
        QString group = QUrl::fromPercentEncoding(pathParts[2].toUtf8());
        // Ensure group has brackets
        if (!group.startsWith('[')) {
            group = "[" + group + "]";
        }
        handleGetPlayer(socket, group);
    } else if (endpoint == "control" && pathParts.size() >= 4) {
        QString group = QUrl::fromPercentEncoding(pathParts[2].toUtf8());
        QString item = QUrl::fromPercentEncoding(pathParts[3].toUtf8());

        // Ensure group has brackets
        if (!group.startsWith('[')) {
            group = "[" + group + "]";
        }

        if (request.method == "GET") {
            handleGetControl(socket, group, item);
        } else if (request.method == "POST") {
            handleSetControl(socket, group, item, request.body);
        } else {
            HttpResponse response;
            response.statusCode = 405;
            response.statusText = "Method Not Allowed";
            QJsonObject json;
            json["error"] = "Method Not Allowed";
            response.body = QJsonDocument(json).toJson();
            sendHttpResponse(socket, response);
        }
    } else {
        HttpResponse response;
        response.statusCode = 404;
        response.statusText = "Not Found";
        QJsonObject json;
        json["error"] = "Endpoint not found";
        response.body = QJsonDocument(json).toJson();
        sendHttpResponse(socket, response);
    }
}

void RestApiServer::handleGetStatus(QTcpSocket* socket, const HttpRequest& request) {
    Q_UNUSED(request);

    QJsonObject json = getAllPlayersStatus();

    HttpResponse response;
    response.body = QJsonDocument(json).toJson();
    sendHttpResponse(socket, response);
}

void RestApiServer::handleGetPlayer(QTcpSocket* socket, const QString& group) {
    QJsonObject json = getPlayerStatus(group);

    if (json.isEmpty()) {
        HttpResponse response;
        response.statusCode = 404;
        response.statusText = "Not Found";
        QJsonObject errorJson;
        errorJson["error"] = "Player not found";
        errorJson["group"] = group;
        response.body = QJsonDocument(errorJson).toJson();
        sendHttpResponse(socket, response);
        return;
    }

    HttpResponse response;
    response.body = QJsonDocument(json).toJson();
    sendHttpResponse(socket, response);
}

void RestApiServer::handleGetControl(QTcpSocket* socket,
        const QString& group,
        const QString& item) {
    ControlObject* pControl = ControlObject::getControl(ConfigKey(group, item));

    if (!pControl) {
        HttpResponse response;
        response.statusCode = 404;
        response.statusText = "Not Found";
        QJsonObject json;
        json["error"] = "Control not found";
        json["group"] = group;
        json["item"] = item;
        response.body = QJsonDocument(json).toJson();
        sendHttpResponse(socket, response);
        return;
    }

    QJsonObject json;
    json["group"] = group;
    json["item"] = item;
    json["value"] = pControl->get();

    HttpResponse response;
    response.body = QJsonDocument(json).toJson();
    sendHttpResponse(socket, response);
}

void RestApiServer::handleSetControl(QTcpSocket* socket,
        const QString& group,
        const QString& item,
        const QByteArray& body) {
    QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        QJsonObject json;
        json["error"] = "Invalid JSON body. Expected {\"value\": <number>}";
        response.body = QJsonDocument(json).toJson();
        sendHttpResponse(socket, response);
        return;
    }

    QJsonObject requestJson = doc.object();
    if (!requestJson.contains("value")) {
        HttpResponse response;
        response.statusCode = 400;
        response.statusText = "Bad Request";
        QJsonObject json;
        json["error"] = "Missing 'value' field in request body";
        response.body = QJsonDocument(json).toJson();
        sendHttpResponse(socket, response);
        return;
    }

    double value = requestJson["value"].toDouble();

    ControlObject* pControl = ControlObject::getControl(ConfigKey(group, item));
    if (!pControl) {
        HttpResponse response;
        response.statusCode = 404;
        response.statusText = "Not Found";
        QJsonObject json;
        json["error"] = "Control not found";
        json["group"] = group;
        json["item"] = item;
        response.body = QJsonDocument(json).toJson();
        sendHttpResponse(socket, response);
        return;
    }

    pControl->set(value);

    QJsonObject json;
    json["success"] = true;
    json["group"] = group;
    json["item"] = item;
    json["value"] = pControl->get(); // Return actual value after setting

    HttpResponse response;
    response.body = QJsonDocument(json).toJson();
    sendHttpResponse(socket, response);
}

QJsonObject RestApiServer::getPlayerStatus(const QString& group) {
    QJsonObject json;

    // Get track info
    TrackPointer pTrack = PlayerInfo::instance().getTrackInfo(group);
    if (pTrack) {
        json["track"] = getTrackMetadata(pTrack.get());
    } else {
        json["track"] = QJsonValue::Null;
    }

    // Get common playback controls
    auto getControlValue = [&group](const QString& item) -> QJsonValue {
        ControlObject* pControl = ControlObject::getControl(ConfigKey(group, item));
        return pControl ? QJsonValue(pControl->get()) : QJsonValue::Null;
    };

    json["play"] = getControlValue("play");
    json["play_indicator"] = getControlValue("play_indicator");
    json["playposition"] = getControlValue("playposition");
    json["duration"] = getControlValue("duration");
    json["volume"] = getControlValue("volume");
    json["pregain"] = getControlValue("pregain");
    json["bpm"] = getControlValue("bpm");
    json["rate"] = getControlValue("rate");
    json["tempo_ratio"] = getControlValue("tempo_ratio");
    json["keylock"] = getControlValue("keylock");
    json["repeat"] = getControlValue("repeat");
    json["loop_enabled"] = getControlValue("loop_enabled");
    json["track_loaded"] = getControlValue("track_loaded");

    return json;
}

QJsonObject RestApiServer::getTrackMetadata(Track* track) {
    if (!track) {
        return QJsonObject();
    }

    QJsonObject json;
    json["artist"] = track->getArtist();
    json["title"] = track->getTitle();
    json["album"] = track->getAlbum();
    json["album_artist"] = track->getAlbumArtist();
    json["genre"] = track->getGenre();
    json["composer"] = track->getComposer();
    json["year"] = track->getYear();
    json["comment"] = track->getComment();
    json["duration"] = track->getDuration();
    json["bpm"] = track->getBpm();
    json["key"] = track->getKeyText();
    json["location"] = track->getLocation();
    json["file_type"] = track->getType();

    return json;
}

QJsonObject RestApiServer::getAllPlayersStatus() {
    QJsonObject json;

    // Get all loaded tracks
    QMap<QString, TrackPointer> loadedTracks = PlayerInfo::instance().getLoadedTracks();

    QJsonArray players;
    for (auto it = loadedTracks.constBegin(); it != loadedTracks.constEnd(); ++it) {
        QString group = it.key();
        QJsonObject playerJson = getPlayerStatus(group);
        playerJson["group"] = group;
        players.append(playerJson);
    }

    json["players"] = players;

    // Add master controls
    auto getControlValue = [](const QString& group, const QString& item) -> QJsonValue {
        ControlObject* pControl = ControlObject::getControl(ConfigKey(group, item));
        return pControl ? QJsonValue(pControl->get()) : QJsonValue::Null;
    };

    QJsonObject master;
    master["volume"] = getControlValue("[Master]", "volume");
    master["balance"] = getControlValue("[Master]", "balance");
    master["headVolume"] = getControlValue("[Master]", "headVolume");
    master["headMix"] = getControlValue("[Master]", "headMix");

    json["master"] = master;

    return json;
}

} // namespace mixxx
