#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>

#include "src/http/httpserver.h"
#include "src/nodes/grinrustnode.h"
#include "src/nodes/grinppnode.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("GrinHttpCtl");
    QCoreApplication::setApplicationVersion("1.0");

    // --- CLI Parser ---
    QCommandLineParser p;
    p.setApplicationDescription("HTTP Controller fuer Grin Rust Node und Grin++");
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
        "Pfad zur Grin Rust Node",
        "path",
        qEnvironmentVariable("GRIN_RUST_BIN")
        );
    QCommandLineOption optRustArg(
        "rust-args",
        "Default-Args Rust (kommagetrennt)",
        "list",
        qEnvironmentVariable("GRIN_RUST_ARGS")
        );
    QCommandLineOption optGppBin(
        "grinpp-bin",
        "Pfad zu Grin++",
        "path",
        qEnvironmentVariable("GRINPP_BIN")
        );
    QCommandLineOption optGppArg(
        "grinpp-args",
        "Default-Args Grin++ (kommagetrennt)",
        "list",
        qEnvironmentVariable("GRINPP_ARGS")
        );
    QCommandLineOption optLogCap(
        "log-cap",
        "Logpuffer-Zeilenzahl (default 5000)",
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

    // --- Optionen auslesen / validieren ---
    bool okPort = false;
    const int portVal = p.value(optPort).toInt(&okPort);
    const quint16 port = (okPort && portVal > 0 && portVal <= 65535) ? quint16(portVal) : quint16(8080);

    bool okCap = false;
    const int capVal = p.value(optLogCap).toInt(&okCap);
    const int logCap = (okCap && capVal > 0) ? capVal : 5000;

    const QString rustBin = p.value(optRustBin);
    const QString gppBin  = p.value(optGppBin);
    const QStringList rustArgs = p.value(optRustArg).split(',', Qt::SkipEmptyParts);
    const QStringList gppArgs  = p.value(optGppArg).split(',', Qt::SkipEmptyParts);

    // --- Nodes konfigurieren ---
    GrinRustNode rust;
    GrinPPNode   grinpp;

    if (!rustBin.isEmpty())  rust.setProgram(rustBin);
    if (!gppBin.isEmpty())   grinpp.setProgram(gppBin);
    if (!rustArgs.isEmpty()) rust.setDefaultArgs(rustArgs);
    if (!gppArgs.isEmpty())  grinpp.setDefaultArgs(gppArgs);


    qDebug()<<"args main: "<<rust.defaultArgs();

    rust.setLogCapacity(logCap);
    grinpp.setLogCapacity(logCap);

    // --- HTTP Server starten ---
    HttpServer http;
    http.registerNode(&rust);
    http.registerNode(&grinpp);

    if (!http.listen(port)) {
        qCritical().noquote() << QString("HTTP Server konnte Port %1 nicht binden.").arg(port);
        return 1;
    }

    qInfo().noquote() << QString("[i] HTTP Server lauscht auf http://0.0.0.0:%1").arg(port);
    qInfo().noquote() << QString("[i] Log-Capacity: %1 Zeilen").arg(logCap);
    if (!rustBin.isEmpty()) qInfo().noquote() << QString("[i] Rust Node:  %1").arg(rustBin);
    if (!gppBin.isEmpty())  qInfo().noquote() << QString("[i] Grin++:     %1").arg(gppBin);

    return app.exec();
}
