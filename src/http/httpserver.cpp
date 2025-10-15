#include "httpserver.h"

/**
 * @brief httpStatusLine
 * @param code
 * @return
 */
static inline QByteArray httpStatusLine(int code)
{
    // Minimal mapping
    switch (code) {
    case 200:
        return "HTTP/1.1 200 OK\r\n";
    case 204:
        return "HTTP/1.1 204 No Content\r\n";
    case 400:
        return "HTTP/1.1 400 Bad Request\r\n";
    case 404:
        return "HTTP/1.1 404 Not Found\r\n";
    case 500:
        return "HTTP/1.1 500 Internal Server Error\r\n";
    default:
        return QByteArray("HTTP/1.1 ") + QByteArray::number(code) + " OK\r\n";
    }
}

/**
 * @brief HttpServer::HttpServer
 * @param parent
 */
HttpServer::HttpServer(QObject *parent) : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &HttpServer::onNewConnection);
}

/**
 * @brief HttpServer::registerNode
 * @param node
 */
void HttpServer::registerNode(INodeController *node)
{
    if (!node) {
        return;
    }
    m_nodes.insert(node->id(), node);
}

/**
 * @brief HttpServer::listen
 * @param port
 * @param addr
 * @return
 */
bool HttpServer::listen(quint16 port, const QHostAddress &addr)
{
    return m_server.listen(addr, port);
}

/**
 * @brief HttpServer::onNewConnection
 */
void HttpServer::onNewConnection()
{
    while (QTcpSocket *s = m_server.nextPendingConnection()) {
        s->setParent(this);

        Request r;
        bool ok = readHttpRequest(s, r);

        if (!ok) {
            writeBadRequest(s);
            s->disconnectFromHost();
            continue;
        }
        routeRequest(s, r);
        s->disconnectFromHost();
    }
}

/**
 * @brief HttpServer::routeRequest
 * @param s
 * @param r
 */
void HttpServer::routeRequest(QTcpSocket *s, const Request &r)
{
    // CORS preflight
    if (r.method == "OPTIONS") {
        handleOptions(s, r);
        return;
    }

    // normalize path (without trailing slash, root)
    QByteArray path = r.path;
    if (path.size() > 1 && path.endsWith('/')) {
        path.chop(1);
    }

    if (r.method == "GET" && path == "/status") {
        handleStatus(s);
        return;
    }

    ///start/{id} (POST)
    if (r.method == "POST" && path.startsWith("/start/")) {
        Request r2 = r;
        r2.idParam = QString::fromUtf8(path.mid(sizeof("/start/") - 1));
        handleStart(s, r2);
        return;
    }

    ///stop/{id} (POST)
    if (r.method == "POST" && path.startsWith("/stop/")) {
        Request r2 = r;
        r2.idParam = QString::fromUtf8(path.mid(sizeof("/stop/") - 1));
        handleStop(s, r2);
        return;
    }

    ///restart/{id} (POST)
    if (r.method == "POST" && path.startsWith("/restart/")) {
        Request r2 = r;
        r2.idParam = QString::fromUtf8(path.mid(sizeof("/restart/") - 1));
        handleRestart(s, r2);
        return;
    }

    ///logs/{id} (GET)
    if (r.method == "GET" && path.startsWith("/logs/")) {
        Request r2 = r;
        r2.idParam = QString::fromUtf8(path.mid(sizeof("/logs/") - 1));
        handleLogs(s, r2);
        return;
    }

    writeNotFound(s);
}

/**
 * @brief HttpServer::handleOptions
 * @param s
 */
void HttpServer::handleOptions(QTcpSocket *s, const Request &)
{
    writeNoContentCors(s);
}

/**
 * @brief HttpServer::handleStatus
 * @param s
 */
void HttpServer::handleStatus(QTcpSocket *s)
{
    QJsonObject root;
    QJsonObject nodes;
    for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
        nodes[it.key()] = it.value()->statusJson();
    }
    root["nodes"] = nodes;
    writeJson(s, 200, root);
}

/**
 * @brief HttpServer::handleStart
 * @param s
 * @param r
 */
void HttpServer::handleStart(QTcpSocket *s, const Request &r)
{
    // get correct node
    auto *n = nodeForId(r.idParam);
    if (!n) {
        writeNotFound(s, "unknown id");
        return;
    }

    QStringList extra;
    bool okJson = false;
    const QJsonObject obj = parseJsonObject(r.body, &okJson);
    if (okJson) {
        const auto a = obj.value(QStringLiteral("args"));
        if (a.isArray()) {
            for (const auto &v : a.toArray()) {
                extra << v.toString();
            }
        } else if (a.isString()) {
            extra = a.toString().split(',', Qt::SkipEmptyParts);
        }
    }

    const bool ok = n->start(extra);

    qDebug() << "ok: " << ok;
    QJsonObject out{{"ok", ok}, {"status", n->statusJson()} };
    writeJson(s, ok ? 200 : 500, out);
}

/**
 * @brief HttpServer::handleStop
 * @param s
 * @param r
 */
void HttpServer::handleStop(QTcpSocket *s, const Request &r)
{
    auto *n = nodeForId(r.idParam);
    if (!n) {
        writeNotFound(s, "unknown id");
        return;
    }

    const bool ok = n->stop();
    QJsonObject out{{"ok", ok}, {"status", n->statusJson()} };
    writeJson(s, ok ? 200 : 500, out);
}

/**
 * @brief HttpServer::handleRestart
 * @param s
 * @param r
 */
void HttpServer::handleRestart(QTcpSocket *s, const Request &r)
{
    auto *n = nodeForId(r.idParam);
    if (!n) {
        writeNotFound(s, "unknown id");
        return;
    }

    QStringList extra;
    bool okJson = false;
    const QJsonObject obj = parseJsonObject(r.body, &okJson);
    if (okJson) {
        const auto a = obj.value(QStringLiteral("args"));
        if (a.isArray()) {
            for (const auto &v : a.toArray()) {
                extra << v.toString();
            }
        } else if (a.isString()) {
            extra = a.toString().split(',', Qt::SkipEmptyParts);
        }
    }

    const bool ok = n->restart(4000, extra);
    QJsonObject out{{"ok", ok}, {"status", n->statusJson()} };
    writeJson(s, ok ? 200 : 500, out);
}

/**
 * @brief HttpServer::handleLogs
 * @param s
 * @param r
 */
void HttpServer::handleLogs(QTcpSocket *s, const Request &r)
{
    auto *n = nodeForId(r.idParam);
    if (!n) {
        writeNotFound(s, "unknown id");
        return;
    }

    int nlines = 200;
    const auto it = r.query.constFind("n");
    if (it != r.query.cend()) {
        bool ok = false;
        int v = it.value().toInt(&ok);
        if (ok && v > 0) {
            nlines = v;
        }
    }

    const QStringList lines = n->lastLogLines(nlines);
    QJsonObject out{
        { "id", r.idParam },
        { "lines", QJsonArray::fromStringList(lines) }
    };
    writeJson(s, 200, out);
}

/**
 * @brief HttpServer::nodeForId
 * @param id
 * @return
 */
INodeController *HttpServer::nodeForId(const QString &id) const
{
    auto it = m_nodes.constFind(id);
    if (it == m_nodes.cend()) {
        return nullptr;
    }
    return it.value();
}

/**
 * @brief HttpServer::parseJsonObject
 * @param body
 * @param okOut
 * @return
 */
QJsonObject HttpServer::parseJsonObject(const QByteArray &body, bool *okOut)
{
    if (okOut) {
        *okOut = false;
    }
    if (body.isEmpty()) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isNull() && doc.isObject()) {
        if (okOut) {
            *okOut = true;
        }
        return doc.object();
    }
    return {};
}

/**
 * @brief HttpServer::parseQuery
 * @param rawQuery
 * @return
 */
QMap<QByteArray, QByteArray> HttpServer::parseQuery(const QByteArray &rawQuery)
{
    QMap<QByteArray, QByteArray> m;
    QUrlQuery q(QString::fromUtf8(rawQuery));
    const auto items = q.queryItems(QUrl::FullyDecoded);
    for (const auto &it : items) {
        m.insert(it.first.toUtf8(), it.second.toUtf8());
    }
    return m;
}

/**
 * @brief HttpServer::readHttpRequest
 * @param s
 * @param outReq
 * @return
 */
bool HttpServer::readHttpRequest(QTcpSocket *s, Request &outReq)
{
    bool log = false;

    // Wait for header
    if (!s->waitForReadyRead(5000)) {
        return false;
    }

    QByteArray data = s->readAll();

    int headerEnd = data.indexOf("\r\n\r\n");
    const int headerEndAlt = data.indexOf("\n\n");
    if (headerEnd < 0 && headerEndAlt >= 0) {
        headerEnd = headerEndAlt + 2; // Fallback
    }
    if (headerEnd < 0) {
        while (headerEnd < 0 && s->waitForReadyRead(2000)) {
            data += s->readAll();
            headerEnd = data.indexOf("\r\n\r\n");
            if (headerEnd < 0) {
                int he2 = data.indexOf("\n\n");
                if (he2 >= 0) {
                    headerEnd = he2 + 2;
                }
            }
        }
        if (headerEnd < 0) {
            return false;
        }
    }

    QByteArray head = data.left(headerEnd);
    QByteArray body = data.mid(headerEnd + 4);
    if (data.mid(headerEnd, 4) == "\n\n") {
        body = data.mid(headerEnd);
    }
    const QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty()) {
        return false;
    }

    const QByteArray requestLine = lines.first().trimmed(); // "GET /status HTTP/1.1"
    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2) {
        return false;
    }

    outReq.method = parts[0].trimmed();

    QByteArray urlPart = parts[1].trimmed(); // no path + ?query
    outReq.httpVersion = (parts.size() >= 3) ? parts[2].trimmed() : "HTTP/1.1";

    // Headers
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines[i].trimmed();
        if (line.isEmpty()) {
            continue;
        }
        int colon = line.indexOf(':');
        if (colon > 0) {
            QByteArray k = line.left(colon).trimmed().toLower();
            QByteArray v = line.mid(colon + 1).trimmed();
            outReq.headers.insert(k, v);
        }
    }

    // Query & Path split
    int qpos = urlPart.indexOf('?');
    QByteArray rawPath = (qpos >= 0) ? urlPart.left(qpos) : urlPart;
    QByteArray rawQuery = (qpos >= 0) ? urlPart.mid(qpos + 1) : QByteArray();

    outReq.path = rawPath;
    outReq.query = parseQuery(rawQuery);

    // Body with Content-Length
    int contentLen = 0;
    bool okLen = false;
    if (outReq.headers.contains("content-length")) {
        contentLen = outReq.headers.value("content-length").toInt(&okLen);
    }
    QByteArray buf = body;
    if (okLen && contentLen > buf.size()) {
        int remain = contentLen - buf.size();
        while (remain > 0 && s->waitForReadyRead(3000)) {
            QByteArray chunk = s->read(qMin(8192, remain));
            buf += chunk;
            remain -= chunk.size();
        }
    }
    outReq.body = buf;

    if (log) {
        qDebug() << "Request:";
        qDebug() << outReq.body;
        qDebug() << outReq.headers;
        qDebug() << outReq.httpVersion;
        qDebug() << outReq.idParam;
        qDebug() << outReq.method;
        qDebug() << outReq.path;
        qDebug() << outReq.query;
    }

    return true;
}

/**
 * @brief HttpServer::writeJson
 * @param s
 * @param statusCode
 * @param obj
 */
void HttpServer::writeJson(QTcpSocket *s, int statusCode, const QJsonObject &obj)
{
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    QByteArray resp;
    resp += httpStatusLine(statusCode);
    resp += "Content-Type: application/json\r\n";
    resp += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
    resp += "Access-Control-Allow-Origin: *\r\n";
    resp += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    resp += "Access-Control-Allow-Headers: Content-Type\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += payload;

    s->write(resp);
    s->flush();
}

/**
 * @brief HttpServer::writeNoContentCors
 * @param s
 */
void HttpServer::writeNoContentCors(QTcpSocket *s)
{
    QByteArray resp;
    resp += httpStatusLine(204);
    resp += "Access-Control-Allow-Origin: *\r\n";
    resp += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    resp += "Access-Control-Allow-Headers: Content-Type\r\n";
    resp += "Connection: close\r\n\r\n";
    s->write(resp);
    s->flush();
}

/**
 * @brief HttpServer::writeNotFound
 * @param s
 * @param msg
 */
void HttpServer::writeNotFound(QTcpSocket *s, const QString &msg)
{
    writeJson(s, 404, QJsonObject{{"error", msg}});
}

/**
 * @brief HttpServer::writeBadRequest
 * @param s
 * @param msg
 */
void HttpServer::writeBadRequest(QTcpSocket *s, const QString &msg)
{
    writeJson(s, 400, QJsonObject{{"error", msg}});
}

/**
 * @brief HttpServer::writeServerError
 * @param s
 * @param msg
 */
void HttpServer::writeServerError(QTcpSocket *s, const QString &msg)
{
    writeJson(s, 500, QJsonObject{{"error", msg}});
}
