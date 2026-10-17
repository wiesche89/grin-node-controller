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

#ifdef Q_OS_WIN
    if (p->isWritable()) {
        p->write("q\n");
        p->waitForBytesWritten(1000);
    }
    if (p->waitForFinished(gracefulMs))
        return true;
#endif
    // Headless Grin handles SIGTERM; stdin is only used by the TUI.
    p->terminate();
    if (p->waitForFinished(gracefulMs))
        return true;
    p->kill();
    return p->waitForFinished(3000);
}
