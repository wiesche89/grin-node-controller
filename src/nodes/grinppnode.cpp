#include "grinppnode.h"

/**
 * @brief GrinPPNode::GrinPPNode
 * @param parent
 */
GrinPPNode::GrinPPNode(QObject *parent) :
    NodeProc("grinpp",
             qEnvironmentVariable("GRINPP_BIN"),
             qEnvironmentVariable("GRINPP_ARGS").split(',', Qt::SkipEmptyParts),
             5000,
             parent)
{
}

/**
 * @brief GrinPPNode::beforeStart
 * @param args
 */
void GrinPPNode::beforeStart(QStringList &args)
{
    Q_UNUSED(args);
    // handle something before start
}
