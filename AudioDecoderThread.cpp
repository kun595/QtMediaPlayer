#include "AudioDecoderThread.h"


AudioDecoderThread::AudioDecoderThread()
{
}

AudioDecoderThread::~AudioDecoderThread(){
    if(audioframe){
        av_frame_free(&audioframe);
    }
    if (audiostream) {
        SDL_DestroyAudioStream(audiostream);
        audiostream = nullptr;
    }
    while(!packetqueue.isEmpty()){
        AVPacket *packet = packetqueue.dequeue();
        av_packet_free(&packet);
    }
}


void AudioDecoderThread::updatpacket(AVPacket *packet){
    QMutexLocker locker(&queuemutex);
    packetqueue.enqueue(packet);
}

void AudioDecoderThread::getcodecctx(AVCodecContext *codecctx){
    audiocodeccontext = codecctx;
}

void AudioDecoderThread::run(){
    audioframe = av_frame_alloc();
    while(!isInterruptionRequested()){
        if(totaltime != 0){
            emit clocktoVideoDecoder(audio_clock);
        }
        if(SEEKFRAME){
            SDL_ClearAudioStream(audiostream);
            avcodec_flush_buffers(audiocodeccontext);
            while(!packetqueue.isEmpty()){
                AVPacket *oldpacket = packetqueue.dequeue();
                av_packet_free(&oldpacket);
            }
            emit audioflush();
            SEEKFRAME = false;
            FORCEDECODE = true;
            continue;
        }
        //暂停
        if(STOP && !FORCEDECODE){
            QMutex waitmutex;
            QMutexLocker locker(&waitmutex);
            stoppress.wait(&waitmutex);
            continue;
        }
        //文件读取结束
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
                send_ret = avcodec_send_packet(audiocodeccontext, packet);
                av_packet_free(&packet);
            }
            while(send_ret >= 0){
                int recv_ret = avcodec_receive_frame(audiocodeccontext, audioframe);
                if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) {
                    // 需要新数据或解码结束
                    av_frame_unref(audioframe);
                    if(recv_ret == AVERROR_EOF){
                        qDebug() << "audio end";
                    }
                    break;
                }
                if(starttime == -1.0){
                    starttime = audioframe->pts * av_q2d(audio_time_base);
                    sysstarttime = av_gettime_relative() / 1000000.0;
                    emit starttimetoVideoDecoder(starttime);
                }
                if(!INITDEVICE){
                    initaudiodevice(audioframe);
                }
                if(INITDEVICE){
                    if(totaltime == 0){
                        //直播时使用系统时钟
                        double audiotimenow = audioframe->pts * av_q2d(audio_time_base) - starttime;
                        double sysclock = av_gettime_relative() / 1000000.0 - sysstarttime;
                        if(audiotimenow - sysclock < -0.1){
                            continue;
                        }
                    }
                    frametostream(audioframe);
                    if(FORCEDECODE){
                        FORCEDECODE = false;
                    }
                }
            }
        }
    }
}


void AudioDecoderThread::initaudiodevice(AVFrame *frame){
    //初始化音频播放系统
    bool init = SDL_Init(SDL_INIT_AUDIO);
    if(init == false){
        qDebug() << "init false" << SDL_GetError();
    }
    //初始化audiostream
    SDL_AudioSpec spec{
        .format =  SDL_AUDIO_S16,
        .channels = frame->ch_layout.nb_channels,
        .freq = frame->sample_rate
    };
    //另一种写法
    // SDL_AudioSpec spec;
    // spec.format = SDL_AUDIO_S16;
    // spec.channels = frame->ch_layout.nb_channels;
    // spec.freq = frame->sample_rate;

    audiostream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioDecoderThread::streamcallback, this);

    if(audiostream == NULL){
        qDebug() << "SDL_OpenAudioDeviceStream failed:"<<SDL_GetError();
    }

    bool isplay = SDL_ResumeAudioStreamDevice(audiostream);
    if(isplay == false){
        qDebug() << "SDL_ResumeAudioStreamDevice failed:" << SDL_GetError();
    }

    int swr_ret = swr_alloc_set_opts2(&swr,         // we're allocating a new context
                                      &frame->ch_layout, // out_ch_layout
                                      AV_SAMPLE_FMT_S16,    // out_sample_fmt
                                      frame->sample_rate,                // out_sample_rate
                                      &frame->ch_layout, // in_ch_layout
                                      (AVSampleFormat)frame->format,   // in_sample_fmt
                                      frame->sample_rate,                // in_sample_rate
                                      0,                    // log_offset
                                      NULL);    // log_ctx



    if(swr_ret < 0){
        qDebug()<<"swr参数配置错误";
    }

    //初始化上下文
    int swrinit_ret = swr_init(swr);
    if(swrinit_ret < 0){
        qDebug()<< "swr初始化失败";
    }

    INITDEVICE = true;

}



void AudioDecoderThread::frametostream(AVFrame *frame){
    //通过转换格式上下文输出采样数字，但我们设置了一样的采样率所以并没有做转化
    int outsample_nums = swr_get_out_samples(swr, frame->nb_samples);

    uint8_t *output_buffer;

    int buffer_ret = av_samples_alloc(&output_buffer,
                                      frame->linesize,
                                      frame->ch_layout.nb_channels,
                                      outsample_nums,
                                      AV_SAMPLE_FMT_S16,
                                      0
                                      );
    if (buffer_ret < 0){
        qDebug() << "初始化outbuffer失败";
    }

    //将原有的数据转化后输出到outbuffer上
    int convertedsample_nums = swr_convert(swr,
                                           &output_buffer,            // 目标缓冲区
                                           outsample_nums,        // 目标容量
                                           frame->data, // 源数据
                                           frame->nb_samples);  // 源样本数

    if(convertedsample_nums < 0){
        qDebug() << "转换失败";
    }


    //计算数据大小 示例:{sample0_channel0, sample0_channel1 .....}
    //所以数据大小 = sample数 * sample字节大小 * 声道数
    int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    int channel_count = frame->ch_layout.nb_channels;
    int data_size = convertedsample_nums * bytes_per_sample * channel_count;
    //把数据载入audiostream中

    bool isputed = SDL_PutAudioStreamData(audiostream, output_buffer, data_size);
    if(isputed == false){
        qDebug() << "SDL_PutAudioStreamData failed:" << SDL_GetError();
    }


    bytepersecond = frame->sample_rate * bytes_per_sample * channel_count;

    av_freep(&output_buffer);
}


void AudioDecoderThread::SDLCALL streamcallback(void *userdata, SDL_AudioStream *stream,
                                               int additional_amount, int total_amount ){
    AudioDecoderThread *ap = (AudioDecoderThread *)userdata;

    //sdl回调函数更新音频时钟
    {
        QMutexLocker locker(&ap->timemutex);
        ap->audio_clock += (total_amount - additional_amount) / (double)ap->bytepersecond;
    }

}


void AudioDecoderThread::setseektime(double time){
    QMutexLocker locker(&timemutex);
    audio_clock = time;
}



void AudioDecoderThread::requestwake(){
    decodeend.wakeAll();
    stoppress.wakeAll();
}

void AudioDecoderThread::changeplaystate(){
    if(!STOP){
        SDL_PauseAudioStreamDevice(audiostream);
        STOP = true;
    }else{
        SDL_ResumeAudioStreamDevice(audiostream);
        STOP = false;
        stoppress.wakeAll();
    }
}


void AudioDecoderThread::seekframe(){
    SEEKFRAME = true;
    DECODEEND = false;
    stoppress.wakeAll();
    decodeend.wakeAll();
}

void AudioDecoderThread::audioend(){
    DECODEEND = true;
}


void AudioDecoderThread::settimebase(AVRational timebase){
    audio_time_base = timebase;
}



void AudioDecoderThread::settotaltime(double time){
    totaltime = time;
}


