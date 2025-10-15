#include "grinrustnode.h"

/**
 * @brief GrinRustNode::GrinRustNode
 * @param parent
 */
GrinRustNode::GrinRustNode(QObject *parent) :
    NodeProc("rust",
             qEnvironmentVariable("GRIN_RUST_BIN"),
             qEnvironmentVariable("GRIN_RUST_ARGS").split(',', Qt::SkipEmptyParts),
             5000,
             parent)
{
}

/**
 * @brief GrinRustNode::beforeStart
 * @param args
 */
void GrinRustNode::beforeStart(QStringList &args)
{
    Q_UNUSED(args);
    // handle something before start
}
