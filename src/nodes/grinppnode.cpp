#include "grinppnode.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
static bool sendCtrlCToProcess(DWORD pid)
{
    // Eigene Reaktion auf Ctrl-C unterdrücken
    SetConsoleCtrlHandler(NULL, TRUE);

    // An Konsole des Child anhängen (bei CREATE_NEW_CONSOLE klappt das),
    // ansonsten wirkt GenerateConsoleCtrlEvent oft trotzdem auf die Gruppe.
    AttachConsole(pid);

    BOOL ok = GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
    Sleep(200);

    FreeConsole();
    SetConsoleCtrlHandler(NULL, FALSE);
    return ok == TRUE;
}

#else
#include <signal.h>
#include <unistd.h>
#endif

GrinPPNode::GrinPPNode(QObject *parent) :
    NodeProc("grinpp",
             qEnvironmentVariable("GRINPP_BIN"),
             qEnvironmentVariable("GRINPP_ARGS").split(',', Qt::SkipEmptyParts),
             5000,
             parent)
{
}

void GrinPPNode::beforeStart(QStringList &args)
{
    Q_UNUSED(args);
    // falls Grin++ Flags für headless/log/pidfile hat, hier ergänzen.
}

bool GrinPPNode::stop(int gracefulMs)
{
    QProcess *p = proc();
    if (p->state() == QProcess::NotRunning) {
        return true;
    }

#ifdef Q_OS_WIN
    // Ctrl+C an Prozessgruppe
    const DWORD pid = static_cast<DWORD>(p->processId());
    sendCtrlCToProcess(pid);
    if (p->waitForFinished(gracefulMs)) {
        return true;
    }

    // Fallback: Ctrl+Break (optional), dann Kill
    // GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
    // if (p->waitForFinished(1000)) return true;

    p->kill();
    return p->waitForFinished(3000);
#else
    // Unix: SIGINT ~ Ctrl+C
    const pid_t pid = static_cast<pid_t>(p->processId());

    if (unixStartedWithSetSid()) {
        // Eigene Session/Gruppe: ganze Gruppe signalisieren
        ::kill(-pid, SIGINT);    // negativ = Prozessgruppe
    } else {
        ::kill(pid, SIGINT);
    }
    if (p->waitForFinished(gracefulMs)) {
        return true;
    }

    // Fallback: SIGTERM → SIGKILL
    if (unixStartedWithSetSid()) {
        ::kill(-pid, SIGTERM);
    } else {
        ::kill(pid, SIGTERM);
    }
    if (p->waitForFinished(1500)) {
        return true;
    }

    if (unixStartedWithSetSid()) {
        ::kill(-pid, SIGKILL);
    } else {
        ::kill(pid, SIGKILL);
    }
    return p->waitForFinished(3000);
#endif
}
