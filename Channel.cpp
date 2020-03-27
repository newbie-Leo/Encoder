#include "Channel.h"
#include "unistd.h"
#include <QFile>
#include "Json.h"

LinkObject* Channel::httpServer=NULL;
LinkObject* Channel::rtspServer=NULL;
LinkObject *Channel::audioMini=NULL;
Channel::Channel(QObject *parent) :
    QObject(parent)
{
    overlay=Link::create("Overlay");
    volume=Link::create("Volume");
    snap=Link::create("EncodeV");
    QVariantMap sd;
    sd["width"]=640;
    sd["height"]=360;
    sd["codec"]="jpeg";
    snap->start(sd);
    enable=false;
    enableAVS=false;
    isSrcLine=false;
    audio=NULL;
    video=NULL;
    encA=NULL;
    encV=NULL;
    encV2=NULL;
    udp_sub=NULL;
    id=-1;
    if(httpServer==NULL)
    {
        httpServer=Link::create("TSHttp");
        QVariantMap httpData;
        httpData["TSBuffer"]=0;
        httpServer->start(httpData);
    }
    if(rtspServer==NULL)
    {
        rtspServer=Link::create("Rtsp");
        rtspServer->start();
    }
}

void Channel::init(QVariantMap)
{
    if(audioMini==NULL)
    {
        if(QFile::exists("/dev/tlv320aic31"))
        {
            audioMini=Link::create("InputAi");
            QVariantMap data;
            data["interface"]="Mini-In";
            audioMini->start(data);
        }
        else
        {
            QVariantMap ifaceA=Json::loadFile("/link/config/board.json").toMap()["interfaceA"].toMap();
            if(ifaceA.keys().contains("Line-In"))
            {
                audioMini=Link::create("InputAi");
                QVariantMap data;
                data["interface"]="Line-In";
                audioMini->start(data);
            }
        }
    }

    if(video!=NULL)
    {
        video->linkV(overlay)->linkV(snap);
    }

    if(audio!=NULL)
    {
        volume->start();
        audio->linkA(volume);
    }

    QVariantMap path;
    if(encA==NULL)
        path["mute"]=true;

    muxMap["rtmp"]=Link::create("Mux");
    path["path"]="rtmp://127.0.0.1/live/stream" + QString::number(id);
    muxMap["rtmp"]->setData(path);

    muxMap["push"]=Link::create("Mux");
    if(encA==NULL)
    {
        path["path"]="rtmp://127.0.0.1/live/test" + QString::number(id);
        muxMap["push"]->setData(path);
    }


    muxMap["hls"]=Link::create("Mux");
    path["path"]="/tmp/hls/stream" + QString::number(id)+".m3u8";
    muxMap["hls"]->setData(path);

    muxMap["srt"]=Link::create("Mux");
    path["path"]="srt://127.0.0.1:8080?streamid=push/live/stream" + QString::number(id);
    muxMap["srt"]->setData(path);


    muxMap["ts"]=Link::create("Mux");
    path["format"]="mpegts";
    path["path"]="mem://stream" + QString::number(id);
    muxMap["ts"]->setData(path);

    muxMap["rtsp"]=Link::create("Mux");
    path["format"]="rtsp";
    path["path"]="mem://stream" + QString::number(id);
    muxMap["rtsp"]->setData(path);

    udp=Link::create("TSUdp");
    muxMap["ts"]->linkV(udp);

    {
        path.remove("format");
        muxMap_sub["rtmp"]=Link::create("Mux");
        path["path"]="rtmp://127.0.0.1/live/sub" + QString::number(id);
        muxMap_sub["rtmp"]->setData(path);

        muxMap_sub["push"]=Link::create("Mux");
        if(encA==NULL)
        {
            path["path"]="rtmp://127.0.0.1/live/test" + QString::number(id);
            muxMap_sub["push"]->setData(path);
        }


        muxMap_sub["hls"]=Link::create("Mux");
        path["path"]="/tmp/hls/sub" + QString::number(id)+".m3u8";
        muxMap_sub["hls"]->setData(path);

        muxMap_sub["srt"]=Link::create("Mux");
        path["path"]="srt://127.0.0.1:8080?streamid=push/live/stream" + QString::number(id);
        muxMap_sub["srt"]->setData(path);

        muxMap_sub["ts"]=Link::create("Mux");
        path["format"]="mpegts";
        path["path"]="mem://sub" + QString::number(id);
        muxMap_sub["ts"]->setData(path);

        muxMap_sub["rtsp"]=Link::create("Mux");
        path["format"]="rtsp";
        path["path"]="mem://sub" + QString::number(id);
        muxMap_sub["rtsp"]->setData(path);

        udp_sub=Link::create("TSUdp");
        muxMap_sub["ts"]->linkV(udp);
    }


    foreach(QString key,muxMap.keys())
    {
        if(encA!=NULL)
        {
            encA->linkA(muxMap[key]);
            encA->linkA(muxMap_sub[key]);
        }
        encV->linkV(muxMap[key]);
        if(encV2!=NULL)
            encV2->linkV(muxMap_sub[key]);
    }
}

void Channel::updateConfig(QVariantMap cfg)
{
    data=cfg;
    enable=data["enable"].toBool();
    bool enable2=data["enable2"].toBool();
    QVariantMap lays;
    lays["lays"]=cfg["overlay"].toList();
    overlay->start(lays);

    if(encV2!=NULL)
    {
        if(enable2)
            encV2->start(cfg["encv2"].toMap());
        else
            encV2->stop();
    }

    {
        QVariantMap muxData;
        if(cfg["enca"].toMap()["codec"].toString()=="close")
            muxData["mute"]=true;
        else
            muxData["mute"]=false;
        foreach(QString key,muxMap.keys())
        {
            muxMap[key]->setData(muxData);
            muxMap_sub[key]->setData(muxData);
        }
    }

    QVariantMap stream;
    stream=cfg["stream"].toMap();
    QVariantMap stream2;
    stream2=cfg["stream2"].toMap();



    if(enable && stream["rtmp"].toBool())
        muxMap["rtmp"]->start();
    else
        muxMap["rtmp"]->stop();

    if(enable && stream["hls"].toBool())
        muxMap["hls"]->start(cfg["hls"].toMap());
    else
        muxMap["hls"]->stop();


    if(enable && stream["srt"].toBool())
        muxMap["srt"]->start();
    else
        muxMap["srt"]->stop();



    if(enable &&  (stream["http"].toBool()  || stream["udp"].toMap()["enable"].toBool()))
    {
        if(cfg.contains("ts"))
        {
            QVariantMap tscfg=cfg["ts"].toMap();
            tscfg["service_name"]=cfg["name"].toString();
            muxMap["ts"]->start(tscfg);
        }
        else
            muxMap["ts"]->start();
    }
    else
        muxMap["ts"]->stop();

    if(enable && stream["rtsp"].toBool() )
        muxMap["rtsp"]->start();
    else
        muxMap["rtsp"]->stop();


    if(enable && stream["http"].toBool())
        muxMap["ts"]->linkV(httpServer);
    else
        muxMap["ts"]->unLinkV(httpServer);

    if(enable && stream["rtsp"].toBool())
    {
        muxMap["rtsp"]->linkV(rtspServer);
        muxMap["rtsp"]->linkA(rtspServer);
    }
    else
    {
        muxMap["rtsp"]->unLinkV(rtspServer);
        muxMap["rtsp"]->unLinkA(rtspServer);
    }


    if(enable && stream["udp"].toMap()["enable"].toBool())
        udp->start(stream["udp"].toMap());
    else
        udp->stop();

    if(enable && stream["push"].toMap()["enable"].toBool())
        muxMap["push"]->start(stream["push"].toMap());
    else
        muxMap["push"]->stop();


    if(encV2!=NULL)
    {
        if(enable2 && stream2["rtmp"].toBool())
            muxMap_sub["rtmp"]->start();
        else
            muxMap_sub["rtmp"]->stop();

        if(enable2 &&stream2["hls"].toBool())
            muxMap_sub["hls"]->start(cfg["hls"].toMap());
        else
            muxMap_sub["hls"]->stop();


        if(enable2 && stream2["srt"].toBool())
            muxMap_sub["srt"]->start();
        else
            muxMap_sub["srt"]->stop();


        if(enable2 && (stream2["http"].toBool()  || stream2["udp"].toMap()["enable"].toBool()))
        {
            if(cfg.contains("ts"))
            {
                QVariantMap tscfg=cfg["ts"].toMap();
                tscfg["service_name"]=cfg["name"].toString();
                muxMap_sub["ts"]->start(tscfg);
            }
            else
                muxMap_sub["ts"]->start();
        }
        else
            muxMap_sub["ts"]->stop();

        if(enable2 && stream2["rtsp"].toBool() )
            muxMap_sub["rtsp"]->start();
        else
            muxMap_sub["rtsp"]->stop();


        if(enable2 && stream2["http"].toBool())
            muxMap_sub["ts"]->linkV(httpServer);
        else
            muxMap_sub["ts"]->unLinkV(httpServer);

        if(enable2 && stream2["rtsp"].toBool())
        {
            muxMap_sub["rtsp"]->linkV(rtspServer);
            muxMap_sub["rtsp"]->linkA(rtspServer);
        }
        else
        {
            muxMap_sub["rtsp"]->unLinkV(rtspServer);
            muxMap_sub["rtsp"]->unLinkA(rtspServer);
        }



        if(enable2 && stream2["udp"].toMap()["enable"].toBool())
            udp_sub->start(stream2["udp"].toMap());
        else
            udp_sub->stop();

        if(enable2 && stream2["push"].toMap()["enable"].toBool())
            muxMap_sub["push"]->start(stream2["push"].toMap());
        else
            muxMap_sub["push"]->stop();
    }
}

void Channel::doSnap()
{
    QString path="/tmp/snap/snap"+QString::number(id)+".jpg";
    snap->invoke("snap",path);
}
