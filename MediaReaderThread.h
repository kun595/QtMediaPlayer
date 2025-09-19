#ifndef MEDIAREADERTHREAD_H
#define MEDIAREADERTHREAD_H

#include <QThread>
#include <QDebug>
#include <QMutex>
#include <QWaitCondition>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}


class MediaReaderThread : public QThread
{
    Q_OBJECT
public:
    explicit MediaReaderThread(QString filepath);
    ~MediaReaderThread();
    // double getvideototaltime();
    void changeplaystate();
    void seek(double seektime);
    void requestwake();
    double getkeyframedelay();
public slots:
    void videohadflush();
    void audiohadflush();
    void forceseend();
    void readend();
protected:
    void run();

private:
    //format
    AVFormatContext *formatcontext;
    QString filepath;

    //video
    int videoIndex = -1;
    const AVCodec *videocodec = NULL;
    AVCodecContext *videocodeccontext = nullptr;
    AVRational video_time_base;


    //audio
    int audioIndex = -1;
    const AVCodec *audiocodec;
    AVCodecContext *audiocodeccontext;
    AVRational audio_time_base;

    //subtitle filter
    int subtitleIndex = -1;
    AVFilterContext *buffersrcContext = nullptr;
    AVFilterContext *buffersinkContext = nullptr;
    AVFrame *filter_frame;
    AVFilterGraph *filterGraph = nullptr;

    //packet
    AVPacket *packet;
    bool FORCESEEND = false;
    int unrefpacketcount = 0;

    //time
    double totaltime;
    double keyframedelay = 0;
    int fps;

    //stop
    bool STOP = false;
    QWaitCondition stoppress;
    bool STOPREAD = false;
    QWaitCondition stopread;
    bool livestopwake = false;

    //seek
    bool SEEK = false;
    int64_t seekpts;
    QWaitCondition seekpress;




    void initffmpeg(QString filepath);
    void initfilter();
signals:
    //video
    void codecctxtoVideoDecoder(AVCodecContext *codecctx);
    void totaltimetoVideoDecoder(double time);
    void timebasetoVideoDecoder(AVRational timebase);
    void packettoVideoDecoder(AVPacket *packet);
    void seekvideo();
    void fpstoVideoDecoder(int fps);
    void filtercontexttoVideoDecoder(AVFilterContext *srccontext, AVFilterContext *sinkcontex);
    //window
    void totaltimetowindow(double time);
    void keyframedelaytowindow(double delay);
    //audio
    void codecctxtoAudioDecoder(AVCodecContext *codecctx);
    void packettoAudioDecoder(AVPacket *packet);
    void seekaudio();
    void timebasetoAudioDecoder(AVRational timebase);
    void totaltimetoAudioDecoder(double time);
};

#endif // MEDIAREADERTHREAD_H
