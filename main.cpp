#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include "httpserver.h"
#include "grinrustnode.h"
#include "grinppnode.h"

int main(int argc, char *argv[])
{
    // -------------------------------------------------------------------------------------------------------
    // Setting Application
    // -------------------------------------------------------------------------------------------------------
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("grin-node-controller");
    QCoreApplication::setApplicationVersion("0.0.1");

    // -------------------------------------------------------------------------------------------------------
    // CLI Parser
    // Windows cmd args: --port 8080 --rust-bin C:/grin/grin/grin.exe --grinpp-bin C:/grin/grin/grinnode.exe
    // -------------------------------------------------------------------------------------------------------
    QCommandLineParser p;
    p.setApplicationDescription("Grin Node Controller for Grin Rust Node and Grin++");
    p.addHelpOption();
    p.addVersionOption();

    QCommandLineOption optPort(
        QStringList() << "p" << "port",
            "HTTP Port (default 8080)",
            "port",
            "8080"
        );
    QCommandLineOption optRustBin(
        "rust-bin",
        "Path to Grin Rust Node",
        "path",
        qEnvironmentVariable("GRIN_RUST_BIN")
        );
    QCommandLineOption optRustArg(
        "rust-args",
        "Default-Args Rust",
        "list",
        qEnvironmentVariable("GRIN_RUST_ARGS")
        );
    QCommandLineOption optGppBin(
        "grinpp-bin",
        "Path to Grin++",
        "path",
        qEnvironmentVariable("GRINPP_BIN")
        );
    QCommandLineOption optGppArg(
        "grinpp-args",
        "Default-Args Grin++",
        "list",
        qEnvironmentVariable("GRINPP_ARGS")
        );
    QCommandLineOption optLogCap(
        "log-cap",
        "Logpuffer (default 5000)",
        "n",
        "5000"
        );

    p.addOption(optPort);
    p.addOption(optRustBin);
    p.addOption(optRustArg);
    p.addOption(optGppBin);
    p.addOption(optGppArg);
    p.addOption(optLogCap);
    p.process(app);

    // -------------------------------------------------------------------------------------------------------
    // Option validation
    // -------------------------------------------------------------------------------------------------------
    bool okPort = false;
    const int portVal = p.value(optPort).toInt(&okPort);
    const quint16 port = (okPort && portVal > 0 && portVal <= 65535) ? quint16(portVal) : quint16(8080);

    bool okCap = false;
    const int capVal = p.value(optLogCap).toInt(&okCap);
    const int logCap = (okCap && capVal > 0) ? capVal : 5000;

    const QString rustBin = p.value(optRustBin);
    const QString gppBin = p.value(optGppBin);
    const QStringList rustArgs = p.value(optRustArg).split(',', Qt::SkipEmptyParts);
    const QStringList gppArgs = p.value(optGppArg).split(',', Qt::SkipEmptyParts);

    // -------------------------------------------------------------------------------------------------------
    // Nodes configuration
    // -------------------------------------------------------------------------------------------------------
    GrinRustNode rust;
    GrinPPNode grinpp;

    if (!rustBin.isEmpty()) {
        rust.setProgram(rustBin);
    }
    if (!gppBin.isEmpty()) {
        grinpp.setProgram(gppBin);
    }
    if (!rustArgs.isEmpty()) {
        rust.setDefaultArgs(rustArgs);
    }
    if (!gppArgs.isEmpty()) {
        grinpp.setDefaultArgs(gppArgs);
    }
    rust.setLogCapacity(logCap);
    grinpp.setLogCapacity(logCap);

    // ----------------------------
    // DataDirs
    // ----------------------------
    // Grin++: Home (~/.GrinPP), persistent via Volume
    QString grinppDataDir = qEnvironmentVariable("GRINPP_DATADIR");
    if (grinppDataDir.isEmpty()) {
        // fallback: Standard-Home Grin++ Container
        grinppDataDir = QDir::homePath() + "/.GrinPP";
    }
    grinpp.setDataDir(grinppDataDir);

    // Grin-Rust: ~/.grin/main, persistent via Volume
    QString rustDataDir = qEnvironmentVariable("GRIN_RUST_DATADIR");
    if (rustDataDir.isEmpty()) {
        rustDataDir = QDir::homePath() + "/.grin/main";
    }
    rust.setDataDir(rustDataDir);

    // -------------------------------------------------------------------------------------------------------
    // HTTP Server start
    // -------------------------------------------------------------------------------------------------------
    HttpServer http;
    http.registerNode(&rust);
    http.registerNode(&grinpp);

    if (!http.listen(port)) {
        qCritical().noquote() << QString("HTTP server could not bind to port %1.").arg(port);
        return 1;
    }

    qInfo().noquote() << QString("[i] HTTP server listens on http://0.0.0.0:%1").arg(port);
    qInfo().noquote() << QString("[i] Log-Capacity: %1 rows").arg(logCap);
    if (!rustBin.isEmpty()) {
        qInfo().noquote() << QString("[i] Rust Node:  %1").arg(rustBin);
    }
    if (!gppBin.isEmpty()) {
        qInfo().noquote() << QString("[i] Grin++:     %1").arg(gppBin);
    }

    return app.exec();
}
