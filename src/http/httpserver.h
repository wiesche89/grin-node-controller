#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QJsonObject>
#include <QByteArray>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QScopedPointer>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QNetworkReply>

#include "inodecontroller.h"

class HttpServer : public QObject
{
    Q_OBJECT
public:
    explicit HttpServer(QObject *parent = nullptr);

    void registerNode(INodeController *node); // id
    bool listen(quint16 port = 8080, const QHostAddress &addr = QHostAddress::Any);

private slots:
    void onNewConnection();

private:
    struct Request {
        QByteArray method;                      // "GET", "POST", "OPTIONS", ...
        QByteArray path;                        // "/start/grinpp" "/start/grinrust"
        QByteArray httpVersion;                 // "HTTP/1.1"
        QMap<QByteArray, QByteArray> headers;    // lower-case keys
        QByteArray body;                        // Body
        QMap<QByteArray, QByteArray> query;      // Query-Parameter (roh)
        QString idParam;                        // Path-Parameter /start/{id}, /stop/{id}, /restart/{id}, /logs/{id}
    };

    // Parsing/IO
    static bool readHttpRequest(QTcpSocket *s, Request &outReq);
    static void writeJson(QTcpSocket *s, int statusCode, const QJsonObject &obj);
    static void writeNoContentCors(QTcpSocket *s);
    static void writeNotFound(QTcpSocket *s, const QString &msg = QStringLiteral("not found"));
    static void writeBadRequest(QTcpSocket *s, const QString &msg = QStringLiteral("bad request"));
    static void writeServerError(QTcpSocket *s, const QString &msg = QStringLiteral("server error"));
    void writeNoContentCors(QTcpSocket *s, const QByteArray &allowHeaders = QByteArray("Content-Type, Authorization"));

    // Routing
    void routeRequest(QTcpSocket *s, const Request &r);

    // Endpoint handlers
    void handleOptions(QTcpSocket *s, const Request &r);
    void handleStatus(QTcpSocket *s);
    void handleStart(QTcpSocket *s, const Request &r);
    void handleStop(QTcpSocket *s, const Request &r);
    void handleRestart(QTcpSocket *s, const Request &r);
    void handleLogs(QTcpSocket *s, const Request &r);

    // Handle Proxy
    void handleOwnerProxy(QTcpSocket *s, const Request &r);
    void handleForeignProxy(QTcpSocket *s, const Request &r);
    bool anyNodeRunning() const;
    void proxyToUrl(QTcpSocket *s, const QString &url, const Request &r, const QString &apiKey = QString());
    void writeJsonRaw(QTcpSocket *s, int statusCode, const QByteArray &payload);
    INodeController *firstRunningNode() const;
    QByteArray makeBasicAuthHeader(const QString &password) const;

    // Helper functions
    static QJsonObject parseJsonObject(const QByteArray &body, bool *okOut = nullptr);
    static QMap<QByteArray, QByteArray> parseQuery(const QByteArray &rawQuery);
    INodeController *nodeForId(const QString &id) const;

private:
    QTcpServer m_server;
    QMap<QString, INodeController *> m_nodes; // id -> controller
};

#endif // HTTPSERVER_H
