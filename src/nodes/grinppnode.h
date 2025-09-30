#ifndef GRINPPNODE_H
#define GRINPPNODE_H

#include "NodeProc.h"

class GrinPPNode : public NodeProc {
    Q_OBJECT
public:
    explicit GrinPPNode(QObject* parent = nullptr);
protected:
    void beforeStart(QStringList& args) override;
};

#endif // GRINPPNODE_H
