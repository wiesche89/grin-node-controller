#include "grinrustnode.h"
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif
#include <QDebug>

GrinRustNode::GrinRustNode(QObject *parent) :
    NodeProc("rust",
             qEnvironmentVariable("GRIN_RUST_BIN"),
             qEnvironmentVariable("GRIN_RUST_ARGS").split(',', Qt::SkipEmptyParts),
             5000,
             parent)
{
}

void GrinRustNode::beforeStart(QStringList &args)
{
    Q_UNUSED(args);
    // ggf. Default-Args ergänzen
}

bool GrinRustNode::stop(int gracefulMs)
{
    QProcess *p = proc();
    if (p->state() == QProcess::NotRunning) {
        return true;
    }

    // 1) Sanft: "q\n" auf stdin
    if (p->isWritable()) {
        p->write("q\n");
        p->waitForBytesWritten(1000);  // 1 Sekunde reicht völlig
    }
    if (p->waitForFinished(gracefulMs)) {
        return true;
    }

#ifdef Q_OS_WIN
    // 2) Windows: terminate() (Rust-Node reagiert meist)
    p->terminate();
    if (p->waitForFinished(1500)) {
        return true;
    }
    p->kill();
    return p->waitForFinished(3000);
#else
    // 2) Unix: SIGTERM, dann SIGKILL
    ::kill(static_cast<pid_t>(p->processId()), SIGTERM);
    if (p->waitForFinished(1500)) {
        return true;
    }

    ::kill(static_cast<pid_t>(p->processId()), SIGKILL);
    return p->waitForFinished(3000);
#endif
}
