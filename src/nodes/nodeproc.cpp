#include "nodeproc.h"
#include <QJsonArray>

static QString sanitizeLine(const QByteArray& ba) {
    QString s = QString::fromLocal8Bit(ba);
    s.replace("\r\n", "\n");
    s.replace('\r', '\n');
    return s;
}

NodeProc::NodeProc(QString id, QString program, QStringList defaultArgs, int logCapacityLines, QObject* parent)
    : QObject(parent), m_id(std::move(id)), m_program(std::move(program)), m_defaultArgs(std::move(defaultArgs)),
    m_logCapacity(qMax(100, logCapacityLines)), m_logBuffer(m_logCapacity) {
    hookProcess();
}

void NodeProc::hookProcess() {
    QObject::connect(&m_proc, &QProcess::readyReadStandardOutput, this, [this]{ appendLog(m_proc.readAllStandardOutput()); });
    QObject::connect(&m_proc, &QProcess::readyReadStandardError, this, [this]{ appendLog(m_proc.readAllStandardError()); });
    QObject::connect(&m_proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus es){ emit stopped(m_id, code, es); });
    QObject::connect(&m_proc, &QProcess::started, this, [this]{
        QWriteLocker g(&m_lock);
        m_startedAt = QDateTime::currentDateTime();
        emit started(m_id, m_proc.processId());
    });
}

void NodeProc::appendLog(const QByteArray& chunk) {
    QString s = sanitizeLine(chunk);
    const QStringList lines = s.split('\n', Qt::KeepEmptyParts);
    QWriteLocker g(&m_lock);
    for (const QString& line : lines) {
        if (line.isEmpty()) continue;
        m_logBuffer[(m_logStart + m_logSize) % m_logCapacity] = line;
        if (m_logSize < m_logCapacity) ++m_logSize; else m_logStart = (m_logStart + 1) % m_logCapacity;
    }
    emit logUpdated(m_id);
}

bool NodeProc::start(const QStringList& extraArgs) {
    QWriteLocker g(&m_lock);
    if (m_proc.state() != QProcess::NotRunning) return true;
    if (m_program.isEmpty()) return false;
    QStringList args = m_defaultArgs;
    if (!extraArgs.isEmpty()) args += extraArgs;
    beforeStart(args);
    m_proc.setProcessChannelMode(QProcess::MergedChannels);
    m_proc.start(m_program, args, QIODevice::ReadOnly);
    return m_proc.waitForStarted(3000);
}

bool NodeProc::stop(int gracefulMs) {
    QWriteLocker g(&m_lock);
    if (m_proc.state() == QProcess::NotRunning) return true;
    m_proc.terminate();
    if (!m_proc.waitForFinished(gracefulMs)) {
        m_proc.kill();
        return m_proc.waitForFinished(3000);
    }
    return true;
}

bool NodeProc::restart(int gracefulMs, const QStringList& extraArgs) {
    stop(gracefulMs);
    return start(extraArgs);
}

QString NodeProc::program() const {
    QReadLocker g(&m_lock);
    return m_program;
}

void NodeProc::setProgram(const QString& path) {
    QWriteLocker g(&m_lock);
    m_program = path;
}

QStringList NodeProc::defaultArgs() const {
    QReadLocker g(&m_lock);
    return m_defaultArgs;
}

void NodeProc::setDefaultArgs(const QStringList& args) {
    QWriteLocker g(&m_lock);
    m_defaultArgs = args;
}

QJsonObject NodeProc::statusJson() const {
    QReadLocker g(&m_lock);
    QJsonObject o;
    o["id"] = m_id;
    o["running"] = (m_proc.state() != QProcess::NotRunning);
    o["pid"] = static_cast<qint64>(o["running"].toBool() ? m_proc.processId() : 0);
    o["exitCode"] = o["running"].toBool() ? 0 : m_proc.exitCode();
    o["program"] = m_program;
    o["args"] = QJsonArray::fromStringList(m_defaultArgs);
    if (m_startedAt.isValid()) {
        o["startedAt"] = m_startedAt.toUTC().toString(Qt::ISODate);
        o["uptimeSec"] = o["running"].toBool() ? m_startedAt.secsTo(QDateTime::currentDateTime()) : 0;
    } else {
        o["startedAt"] = QJsonValue();
        o["uptimeSec"] = 0;
    }
    return o;
}

QStringList NodeProc::lastLogLines(int n) const {
    QReadLocker g(&m_lock);
    n = qBound(1, n, m_logSize);
    QStringList out; out.reserve(n);
    int start = (m_logStart + (m_logSize - n + m_logCapacity)) % m_logCapacity;
    for (int i = 0; i < n; ++i) out << m_logBuffer[(start + i) % m_logCapacity];
    return out;
}

void NodeProc::setLogCapacity(int capacityLines) {
    QWriteLocker g(&m_lock);
    m_logCapacity = qMax(100, capacityLines);
    m_logBuffer = QVector<QString>(m_logCapacity);
    m_logStart = 0; m_logSize = 0;
}
