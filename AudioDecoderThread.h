#ifndef AUDIODECODERTHREAD_H
#define AUDIODECODERTHREAD_H

#include <QThread>
#include <QDebug>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
}

class AudioDecoderThread : public QThread
{
    Q_OBJECT
public:
    explicit AudioDecoderThread();
    ~AudioDecoderThread();
    void changeplaystate();
    void requestwake();
public slots:
    void updatpacket(AVPacket *packet);
    void getcodecctx(AVCodecContext *codecctx);
    void setseektime(double time);
    void seekframe();
    void audioend();
    void settimebase(AVRational timebase);
    void settotaltime(double time);
protected:
    void run();

private:
    //packet
    AVPacket *packet;
    QQueue<AVPacket *>packetqueue;
    QMutex queuemutex;
    QWaitCondition dequeue;

    //audio
    AVFrame *audioframe;
    AVCodecContext *audiocodeccontext = nullptr;
    SDL_AudioStream *audiostream;
    bool INITDEVICE = false;

    //swr
    SwrContext *swr = nullptr;

    //seek
    QWaitCondition seekpress;
    bool forceDecodeFrame = false;
    bool SEEKFRAME = false;
    bool FORCEDECODE = false;

    //time
    AVRational audio_time_base;
    double bytepersecond;
    double audio_clock = 0.0;
    double starttime = -1.0;
    QMutex timemutex;
    double totaltime;
    double sysstarttime = 0;

    //stop
    bool STOP = false;
    QWaitCondition stoppress;
    bool DECODEEND = false;
    QWaitCondition decodeend;


    void initaudiodevice(AVFrame *frame);
    void frametostream(AVFrame *frame);
    static void SDLCALL streamcallback(void *userdata, SDL_AudioStream *stream,
                                       int additional_amount, int total_amount);
signals:
    void clocktoVideoDecoder(double time);
    void starttimetoVideoDecoder(double time);
    void audioflush();
};

#endif // AUDIODECODERTHREAD_H
