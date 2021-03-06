#ifndef PUSH_H
#define PUSH_H

#include <QObject>
#include "Link.h"
#include <QDateTime>
#include <QTimer>

class Push : public QObject
{
    Q_OBJECT
public:
    explicit Push(QObject *parent = 0);
    void init();
private:
    QVariantMap config;
    LinkObject *srcA;
    LinkObject *srcV;

    LinkObject *lastSrcA;
    LinkObject *lastSrcV;
    struct PushUrl
    {
        QString path;
        LinkObject *mux;
        bool enable;
    };
    QList<PushUrl*> urlList;
    QDateTime startTime;
    bool bPushing;
    LinkObject *preview;


signals:

public slots:
    bool start();
    bool stop();
    QVariantMap getState();
    bool update(QString json);
};
extern Push *GPush;
#endif // PUSH_H
