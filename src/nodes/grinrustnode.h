#ifndef GRINRUSTNODE_H
#define GRINRUSTNODE_H

#include <QProcessEnvironment>

#include "nodeproc.h"

class GrinRustNode : public NodeProc
{
    Q_OBJECT
public:
    explicit GrinRustNode(QObject *parent = nullptr);
protected:
    void beforeStart(QStringList &args) override;
};

#endif // GRINRUSTNODE_H
