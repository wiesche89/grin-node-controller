#ifndef GRINPPNODE_H
#define GRINPPNODE_H

#include <QProcessEnvironment>

#include "nodeproc.h"

class GrinPPNode : public NodeProc
{
    Q_OBJECT
public:
    explicit GrinPPNode(QObject *parent = nullptr);
protected:
    void beforeStart(QStringList &args) override;
};

#endif // GRINPPNODE_H
