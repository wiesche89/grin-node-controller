#ifndef GRINPPNODE_H
#define GRINPPNODE_H

#include <QProcessEnvironment>

#include "nodeproc.h"

class GrinPPNode : public NodeProc
{
    Q_OBJECT
public:
    explicit GrinPPNode(QObject *parent = nullptr);
    bool stop(int gracefulMs = 4000) override;

protected:
    void beforeStart(QStringList &args) override;
};

#endif // GRINPPNODE_H
