#ifndef NODEPROC_H
#define NODEPROC_H

#include <QObject>
#include <QProcess>
#include <QDateTime>
#include <QVector>
#include <QReadWriteLock>
#include <QJsonObject>
#include <QStringList>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>

#include "inodecontroller.h"

class NodeProc : public QObject, public INodeController
{
    Q_OBJECT
public:
    explicit NodeProc(QString id, QString program =
    {
    }, QStringList defaultArgs = {}, int logCapacityLines = 5000, QObject *parent = nullptr);

    // INodeController
    bool start(const QStringList &extraArgs = {}) override;
    bool stop(int gracefulMs = 4000) override;
    bool restart(int gracefulMs = 4000, const QStringList &extraArgs = {}) override;

    QString id() const override;

    QString program() const override;
    void setProgram(const QString &path) override;
    QStringList defaultArgs() const override;
    void setDefaultArgs(const QStringList &args) override;

    QJsonObject statusJson() const override;
    QStringList lastLogLines(int n) const override;

    void setLogCapacity(int capacityLines);

signals:
    void started(QString id, qint64 pid);
    void stopped(QString id, int exitCode, QProcess::ExitStatus es);
    void logUpdated(QString id);

protected:
    virtual void beforeStart(QStringList &args)
    {
        Q_UNUSED(args);
    }

    QProcess *proc()
    {
        return &m_proc;
    }

    const QProcess *proc() const
    {
        return &m_proc;
    }

    bool unixStartedWithSetSid() const
    {
        return m_unixSetSid;
    }

    void setDataDir(const QString &dir)
    {
        QWriteLocker g(&m_lock);
        m_dataDir = dir;
    }

    QString dataDir() const
    {
        QReadLocker g(&m_lock);
        return m_dataDir;
    }

private:
    void appendLog(const QByteArray &chunk);

    QString m_id;
    QString m_program;
    QStringList m_defaultArgs;
    QString m_dataDir;

    mutable QReadWriteLock m_lock;
    QProcess m_proc;
    QDateTime m_startedAt;

    // Ringpuffer
    int m_logCapacity;
    QVector<QString> m_logBuffer;
    int m_logStart = 0;
    int m_logSize = 0;

    bool m_unixSetSid = false;
};

#endif // NODEPROC_H
