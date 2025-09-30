#include "grinppnode.h"
#include <QProcessEnvironment>

GrinPPNode::GrinPPNode(QObject* parent)
    : NodeProc("grinpp",
               qEnvironmentVariable("GRINPP_BIN"),
               qEnvironmentVariable("GRINPP_ARGS").split(',', Qt::SkipEmptyParts),
               5000,
               parent) {}

void GrinPPNode::beforeStart(QStringList& args) {
    Q_UNUSED(args);
    // Platz für Grin++-spezifische Vorbereitungen
}
