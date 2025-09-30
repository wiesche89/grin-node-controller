#include "grinrustnode.h"
#include <QProcessEnvironment>

GrinRustNode::GrinRustNode(QObject* parent)
    : NodeProc("rust",
               qEnvironmentVariable("GRIN_RUST_BIN"),
               qEnvironmentVariable("GRIN_RUST_ARGS").split(',', Qt::SkipEmptyParts),
               5000,
               parent) {}

void GrinRustNode::beforeStart(QStringList& args) {
    Q_UNUSED(args);
    // Hier könnte man ENV setzen, z. B.:
    // QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // env.insert("GRIN_HOME", "/data");
    // process()->setProcessEnvironment(env);  // (wenn man m_proc exponiert)
}
