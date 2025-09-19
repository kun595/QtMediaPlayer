#ifndef VIDEODECODERTHREAD_H
#define VIDEODECODERTHREAD_H

#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QDebug>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/time.h>
}

class VideoDecoderThread : public QThread
{
    Q_OBJECT
public:
    explicit VideoDecoderThread();
    ~VideoDecoderThread();
    void changeplaystate();
    void dequeuewake();
    void requestwake();
protected:
    void run();
public slots:
    void updatpacket(AVPacket *packet);
    void setcodecctx(AVCodecContext *codecctx);
    void updateAudioClock(double clock);
    void settotaltime(double time);
    void settimebase(AVRational timebase);
    void setstarttime(double time);
    void seekframe();
    void setfiltercontext(AVFilterContext *srccontext, AVFilterContext *sinkcontext);
    void setqueuesize(int fps);
private:
    //video
    AVCodecContext *videocodeccontext = nullptr;
    AVFrame *videoframe;

    //subtitle filter
    AVFilterContext *buffersrcContext = nullptr;
    AVFilterContext *buffersinkContext = nullptr;
    AVFrame *filterframe;

    //packet
    QQueue<AVPacket *>packetqueue;
    QWaitCondition dequeue;
    QMutex queuemutex;
    int queuelength;

    //time
    QMutex clockmutex;
    double audio_clock = 0.0;
    double totaltime;
    int64_t livefirstpts = 0;
    AVRational video_time_base = {0,0};
    double starttime = 0.0;
    double sysstarttime = 0.0;

    //seek
    bool SEEKFRAME = false;
    bool FORCEDECODE = false;

    //stop
    bool STOP = false;
    QWaitCondition stoppress;
    bool DECODEEND = false;
    QWaitCondition decodeend;

    void AVsync(AVFrame *frame);
signals:
    void frametogl(AVFrame *frame);
    void timetowindow(double time);
    void videoflush();
    void needpacket();
    void seektimetoclock(double time);
    void seeksuccess();
    void decodeendtoaudio();
    void decodeendtoreader();
};

#endif // VIDEODECODERTHREAD_H
