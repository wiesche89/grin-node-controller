#ifndef INODECONTROLLER_H
#define INODECONTROLLER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>


class INodeController {
public:
    virtual ~INodeController() = default;


    virtual bool start(const QStringList& extraArgs = {}) = 0;
    virtual bool stop(int gracefulMs = 4000) = 0;
    virtual bool restart(int gracefulMs = 4000, const QStringList& extraArgs = {}) = 0;


    virtual QString id() const = 0; // "rust" oder "grinpp"
    virtual QString program() const = 0;
    virtual void setProgram(const QString& path) = 0;
    virtual QStringList defaultArgs() const = 0;
    virtual void setDefaultArgs(const QStringList& args) = 0;


    virtual QJsonObject statusJson() const = 0;
    virtual QStringList lastLogLines(int n) const = 0;
};

#endif // INODECONTROLLER_H
