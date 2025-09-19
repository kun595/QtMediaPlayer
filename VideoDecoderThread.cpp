#include "VideoDecoderThread.h"


VideoDecoderThread::VideoDecoderThread()
{
}

VideoDecoderThread::~VideoDecoderThread(){
    if(videoframe){
        av_frame_free(&videoframe);
    }
    while(!packetqueue.isEmpty()){
        AVPacket *packet = packetqueue.dequeue();
        av_packet_free(&packet);
    }
}
void VideoDecoderThread::updatpacket(AVPacket *packet){
    QMutexLocker locker(&queuemutex);
    if(packetqueue.size() == queuelength){
        dequeue.wait(&queuemutex);
    }
    packetqueue.enqueue(packet);
}

void VideoDecoderThread::setcodecctx(AVCodecContext *codecctx){
    videocodeccontext = codecctx;
}

void VideoDecoderThread::updateAudioClock(double clock){
    QMutexLocker locker(&clockmutex);
    audio_clock = clock;
}
void VideoDecoderThread::setfiltercontext(AVFilterContext *srccontext, AVFilterContext *sinkcontext){
    buffersrcContext = srccontext;
    buffersinkContext = sinkcontext;
}

void VideoDecoderThread::run(){
    videoframe = av_frame_alloc();
    filterframe = av_frame_alloc();
    while(!isInterruptionRequested()){
        if(SEEKFRAME){
            avcodec_flush_buffers(videocodeccontext);
            while(!packetqueue.isEmpty()){
                AVPacket *oldpacket = packetqueue.dequeue();
                av_packet_free(&oldpacket);
            }
            emit videoflush();
            SEEKFRAME = false;
            if(totaltime != 0){
                FORCEDECODE = true;
            }
            continue;
        }
        if(STOP && !FORCEDECODE){
            QMutex waitmutex;
            QMutexLocker locker(&waitmutex);
            stoppress.wait(&waitmutex);
            continue;
        }
        if(DECODEEND && !FORCEDECODE){
            QMutex waitmutex;
            QMutexLocker locker(&waitmutex);
            decodeend.wait(&waitmutex);
            continue;
        }
        if(!packetqueue.isEmpty()){
            int send_ret = -1;
            {
                QMutexLocker locker(&queuemutex);
                AVPacket *packet = packetqueue.dequeue();
                send_ret = avcodec_send_packet(videocodeccontext, packet);
                av_packet_free(&packet);
                dequeue.wakeAll();
            }
            while(send_ret >= 0){
                int recv_ret = avcodec_receive_frame(videocodeccontext, videoframe);
                if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) {
                    // 需要新数据或解码结束
                    av_frame_unref(videoframe);
                    if(STOP && FORCEDECODE){
                        emit needpacket();
                    }
                    if(recv_ret == AVERROR_EOF){
                        DECODEEND = true;
                        qDebug() << "video end";
                        emit decodeendtoaudio();
                        emit decodeendtoreader();
                    }
                    break;
                }
                //如果有字幕
                if(buffersrcContext && buffersinkContext){
                    int filter_add_ret = av_buffersrc_add_frame_flags(buffersrcContext, videoframe, AV_BUFFERSRC_FLAG_KEEP_REF);
                    while(filter_add_ret >= 0){
                        int filter_ret = av_buffersink_get_frame(buffersinkContext, filterframe);
                        if (filter_ret == AVERROR(EAGAIN) || filter_ret == AVERROR_EOF){
                            av_frame_unref(videoframe);
                            av_frame_unref(filterframe);
                            break;
                        }
                        av_frame_unref(videoframe);
                        AVsync(filterframe);
                    }
                }else{
                    AVsync(videoframe);
                }
            }
        }
    }
}




//音频同步视频函数
void VideoDecoderThread::AVsync(AVFrame *frame) {
    if(FORCEDECODE){
        emit frametogl(frame);

        double seektime = frame->pts * av_q2d(video_time_base);

        emit timetowindow(seektime);
        emit seektimetoclock(seektime);

        FORCEDECODE = false;
        emit seeksuccess();
        return;
    }

    double MIN_SYNC_THRESHOLD = 0.04;
    double MAX_SYNC_THRESHOLD = 0.1;



    int64_t pts = frame->pts;
    // 直播时totaltime为零
    if(totaltime == 0){
        if(livefirstpts == 0){
            livefirstpts = frame->pts;
        }
        pts -= livefirstpts;
    }

    if(sysstarttime == 0){
        sysstarttime = av_gettime_relative() / 1000000.0;
    }

    double masterclock = 0;
    if(totaltime != 0){
        QMutexLocker locker(&clockmutex);
        masterclock = audio_clock;
    }else{
        //直播时使用系统时钟
        masterclock = av_gettime_relative() / 1000000.0 - sysstarttime;
    }


    double ptstime;
    //如果是直播
    if(totaltime == 0){
        //有时候看直播会出现[hevc @]Skipping invalid undecodable NALU: 0,音频没跳过就跳过了视频
        //这时候就不能使用firstpts来获得播放的起始时间了，我通过获得音频的起始时间starttime来当作起始时间
        if(starttime != 0){
            ptstime = frame->pts * av_q2d(video_time_base) - starttime;
        }else{
            ptstime = pts * av_q2d(video_time_base);
        }
    }else{
        ptstime = pts * av_q2d(video_time_base);
    }


    double diff = ptstime - masterclock;


    double durationtime = frame->duration * av_q2d(video_time_base);
    double delay = durationtime;

    double AV_SYNC_THRESHOLD = FFMAX(MIN_SYNC_THRESHOLD, FFMIN(MAX_SYNC_THRESHOLD, delay));

    if(diff > AV_SYNC_THRESHOLD){
        //视频时间超前就等待
        delay += diff;
    }


    if(diff < -AV_SYNC_THRESHOLD){
        //视频落后就放弃渲染丢帧
        return;
    }


    emit frametogl(frame);
    usleep(delay * 1000000);
    emit timetowindow(ptstime + durationtime);

}

void VideoDecoderThread::settotaltime(double time){
    totaltime = time;
}

void VideoDecoderThread::settimebase(AVRational timebase){
    video_time_base = timebase;
}

void VideoDecoderThread::setstarttime(double time){
    starttime = time;
    sysstarttime = av_gettime_relative() / 1000000.0;
}


void VideoDecoderThread::seekframe(){
    SEEKFRAME = true;
    DECODEEND = false;
    stoppress.wakeAll();
    decodeend.wakeAll();
}

void VideoDecoderThread::changeplaystate(){
    if(!STOP){
        STOP = true;
    }else{
        STOP = false;
        stoppress.wakeAll();
    }
}

void VideoDecoderThread::dequeuewake(){
    dequeue.wakeAll();
}

void VideoDecoderThread::setqueuesize(int fps){
    queuelength = fps;
}

void VideoDecoderThread::requestwake(){
    decodeend.wakeAll();
    stoppress.wakeAll();
    dequeue.wakeAll();
}


