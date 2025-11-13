#include "nodeproc.h"

/**
 * @brief sanitizeLine
 * @param ba
 * @return
 */
static QString sanitizeLine(const QByteArray &ba)
{
    QString s = QString::fromLocal8Bit(ba);
    s.replace("\r\n", "\n");
    s.replace('\r', '\n');
    return s;
}

/**
 * @brief NodeProc::NodeProc
 * @param id
 * @param program
 * @param defaultArgs
 * @param logCapacityLines
 * @param parent
 */
NodeProc::NodeProc(QString id, QString program, QStringList defaultArgs, int logCapacityLines, QObject *parent) :
    QObject(parent),
    m_id(std::move(id)),
    m_program(std::move(program)),
    m_defaultArgs(std::move(defaultArgs)),
    m_logCapacity(qMax(100, logCapacityLines)),
    m_logBuffer(m_logCapacity)
{
    QObject::connect(&m_proc, &QProcess::readyReadStandardOutput, this, [this] {
        appendLog(m_proc.readAllStandardOutput());
    });
    QObject::connect(&m_proc, &QProcess::readyReadStandardError, this, [this] {
        appendLog(m_proc.readAllStandardError());
    });
    QObject::connect(&m_proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus es) {
        emit stopped(m_id, code, es);
    });
    QObject::connect(&m_proc, &QProcess::started, this, [this] {
        QWriteLocker g(&m_lock);
        m_startedAt = QDateTime::currentDateTime();
        emit started(m_id, m_proc.processId());
    });
}

/**
 * @brief NodeProc::appendLog
 * @param chunk
 */
void NodeProc::appendLog(const QByteArray &chunk)
{
    QString s = sanitizeLine(chunk);
    const QStringList lines = s.split('\n', Qt::KeepEmptyParts);
    QWriteLocker g(&m_lock);
    for (const QString &line : lines) {
        if (line.isEmpty()) {
            continue;
        }
        m_logBuffer[(m_logStart + m_logSize) % m_logCapacity] = line;
        if (m_logSize < m_logCapacity) {
            ++m_logSize;
        } else {
            m_logStart = (m_logStart + 1) % m_logCapacity;
        }
    }
    emit logUpdated(m_id);
}

/**
 * @brief NodeProc::start
 * Starts the node process with optional extra arguments.
 * Ensures thread-safety using read/write locks and does not block unnecessarily.
 * @param extraArgs Additional command-line arguments to pass when starting the process.
 * @return true if the process started successfully or is already running, false otherwise.
 */
bool NodeProc::start(const QStringList &extraArgs)
{
    {
        QWriteLocker g(&m_lock);
        if (m_proc.state() != QProcess::NotRunning) {
            return true;
        }
        if (m_program.isEmpty()) {
            return false;
        }
    }

    QStringList args;
    {
        QReadLocker r(&m_lock);
        args = m_defaultArgs;
    }
    if (!extraArgs.isEmpty()) {
        args += extraArgs;
    }
    beforeStart(args);

    m_proc.setProcessChannelMode(QProcess::ForwardedChannels);
    m_proc.setReadChannel(QProcess::StandardOutput);

#ifdef Q_OS_WIN
    // Eigene Prozessgruppe (für Ctrl+C/Ctrl+Break)
    m_proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        // CREATE_NEW_PROCESS_GROUP
        a->flags |= 0x00000200;

        // Optional: sichtbares Fenster erzwingen, falls gewünscht
        if (qEnvironmentVariable("GRIN_SHOW_CONSOLE") == "1") {
            a->flags |= 0x00000010; // CREATE_NEW_CONSOLE
        }
    });

    m_proc.start(m_program, args, QIODevice::ReadWrite);

#else // UNIX / LINUX / macOS
    // Optional: sichtbares Terminal erzwingen (wenn verfügbar)
    // GRIN_FORCE_TERMINAL=1 verwendet xterm/gnome-terminal/konsole (erste gefundene).
    if (qEnvironmentVariable("GRIN_FORCE_TERMINAL") == "1") {
        QString term;
        if (QFile::exists("/usr/bin/xterm")) {
            term = "/usr/bin/xterm";
        } else if (QFile::exists("/usr/bin/gnome-terminal")) {
            term = "/usr/bin/gnome-terminal";
        } else if (QFile::exists("/usr/bin/konsole")) {
            term = "/usr/bin/konsole";
        }

        if (!term.isEmpty()) {
            QStringList targs;
            if (term.endsWith("xterm")) {
                targs << "-hold" << "-e" << m_program;
                targs += args;
            } else if (term.endsWith("gnome-terminal")) {
                // gnome-terminal erwartet Befehle nach -- :
                targs << "--" << m_program;
                targs += args;
            } else if (term.endsWith("konsole")) {
                targs << "--hold" << "-e" << m_program;
                targs += args;
            } else {
                // Fallback: direkt
                targs << "-e" << m_program;
                targs += args;
            }
            m_unixSetSid = false; // Terminal emu übernimmt
            m_proc.start(term, targs, QIODevice::ReadWrite);
        } else {
            // Kein Terminal gefunden -> normaler Start
            m_unixSetSid = false;
            m_proc.start(m_program, args, QIODevice::ReadWrite);
        }
    } else {
        // Headless, aber mit eigener Session/Gruppe – über /usr/bin/setsid
        if (QFile::exists("/usr/bin/setsid")) {
            QStringList sargs;
            sargs << m_program;
            sargs += args;
            m_unixSetSid = true;
            m_proc.start("/usr/bin/setsid", sargs, QIODevice::ReadWrite);
        } else {
            m_unixSetSid = false;
            m_proc.start(m_program, args, QIODevice::ReadWrite);
        }
    }
#endif

    if (!m_proc.waitForStarted(10000)) {
        return m_proc.state() == QProcess::Running;
    }

    return true;
}

bool NodeProc::stop(int gracefulMs)
{
    qDebug() << "stop start...";
    QWriteLocker g(&m_lock);
    if (m_proc.state() == QProcess::NotRunning) {
        return true;
    }
    m_proc.terminate();
    if (!m_proc.waitForFinished(gracefulMs)) {
        m_proc.kill();
        return m_proc.waitForFinished(3000);
    }

    qDebug() << "stop end...";
    return true;
}

bool NodeProc::restart(int gracefulMs, const QStringList &extraArgs)
{
    stop(gracefulMs);
    return start(extraArgs);
}

QString NodeProc::id() const
{
    return m_id;
}

QString NodeProc::program() const
{
    QReadLocker g(&m_lock);
    return m_program;
}

void NodeProc::setProgram(const QString &path)
{
    QWriteLocker g(&m_lock);
    m_program = path;
}

QStringList NodeProc::defaultArgs() const
{
    QReadLocker g(&m_lock);
    return m_defaultArgs;
}

void NodeProc::setDefaultArgs(const QStringList &args)
{
    QWriteLocker g(&m_lock);
    m_defaultArgs = args;
}

QJsonObject NodeProc::statusJson() const
{
    QReadLocker g(&m_lock);
    QJsonObject o;

    const bool isRunning = (m_proc.state() != QProcess::NotRunning);

    o["id"] = m_id;
    o["running"] = isRunning;
    o["pid"] = static_cast<qint64>(isRunning ? m_proc.processId() : 0);
    o["exitCode"] = isRunning ? 0 : m_proc.exitCode();
    o["program"] = m_program;
    o["args"] = QJsonArray::fromStringList(m_defaultArgs);

    if (m_startedAt.isValid()) {
        o["startedAt"] = m_startedAt.toUTC().toString(Qt::ISODate);
        o["uptimeSec"] = isRunning ? m_startedAt.secsTo(QDateTime::currentDateTime()) : 0;
    } else {
        o["startedAt"] = QJsonValue();
        o["uptimeSec"] = 0;
    }

    //
    // 🔑 API-Keys aus dem DataDir laden
    //
    QString ownerApiKey;
    QString foreignApiKey;

    if (!m_dataDir.isEmpty()) {
        QDir dir(m_dataDir);

        auto readFileIfExists = [](const QString &path) -> QString {
                                    QFile f(path);
                                    if (!f.exists()) {
                                        return QString();
                                    }
                                    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                                        return QString();
                                    }
                                    QByteArray data = f.readAll();
                                    return QString::fromUtf8(data).trimmed();
                                };

        auto readFirstExisting = [&](const QStringList &candidates) -> QString {
                                     for (const QString &name : candidates) {
                                         const QString fullPath = dir.filePath(name);
                                         QFileInfo fi(fullPath);
                                         if (fi.exists() && fi.isFile()) {
                                             QString val = readFileIfExists(fullPath);
                                             if (!val.isEmpty()) {
                                                 return val;
                                             }
                                         }
                                     }
                                     return QString();
                                 };

        // Rust-Grin: .api_secret / .foreign_api_secret
        // Fallback: .secret / .foreignsecret für Custom-Setups
        ownerApiKey = readFirstExisting({ ".api_secret"});
        foreignApiKey = readFirstExisting({ ".foreign_api_secret"});
    }

    // Grin++ hat keine entsprechenden Dateien → ownerApiKey/foreignApiKey bleiben leer
    o["ownerApiKey"] = ownerApiKey;
    o["foreignApiKey"] = foreignApiKey;

    return o;
}

QStringList NodeProc::lastLogLines(int n) const
{
    QReadLocker g(&m_lock);
    n = qBound(1, n, m_logSize);
    QStringList out;
    out.reserve(n);
    int start = (m_logStart + (m_logSize - n + m_logCapacity)) % m_logCapacity;
    for (int i = 0; i < n; ++i) {
        out << m_logBuffer[(start + i) % m_logCapacity];
    }
    return out;
}

void NodeProc::setLogCapacity(int capacityLines)
{
    QWriteLocker g(&m_lock);
    m_logCapacity = qMax(100, capacityLines);
    m_logBuffer = QVector<QString>(m_logCapacity);
    m_logStart = 0;
    m_logSize = 0;
}
