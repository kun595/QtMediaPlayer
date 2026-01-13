#include "MediaReaderThread.h"



MediaReaderThread::MediaReaderThread(QString filepath)
    :formatcontext(nullptr),
    packet(nullptr),
    videocodeccontext(nullptr)
{
    initffmpeg(filepath);
    initfilter();
}

MediaReaderThread::~MediaReaderThread()
{
    if(formatcontext){
        avformat_free_context(formatcontext);
    }
    if(packet){
        av_packet_free(&packet);
    }
    if(videocodeccontext){
        avcodec_free_context(&videocodeccontext);
    }
    if(audiocodeccontext){
        avcodec_free_context(&audiocodeccontext);
    }
    if(buffersrcContext){
        avfilter_free(buffersrcContext);
    }
    if(buffersinkContext){
        avfilter_free(buffersinkContext);
    }
    if(filterGraph){
        avfilter_graph_free(&filterGraph);
    }
}

void MediaReaderThread::initffmpeg(QString path){
    //初始化ffmpeg
    //网络模块初始化
    avformat_network_init();
    //初始化格式和包
    formatcontext = avformat_alloc_context();

    filepath = path;

    AVDictionary *option = NULL;

    if(filepath.contains("douyin")){
        av_dict_set(&option, "headers", "referer: https://www.douyin.com\r\n", 0);
    }

    if(avformat_open_input(&formatcontext, filepath.toUtf8(), NULL, &option) != 0){
        qDebug() << "视频打开失败";
        return;
    }



    //获取stream信息
    if (avformat_find_stream_info(formatcontext, NULL) < 0) {
        qDebug() << "stream打开失败";
        avformat_free_context(formatcontext);
        return;
    }


    // 查找视频流
    for (unsigned int i = 0; i < formatcontext->nb_streams; i++) {
        if (formatcontext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            //有些视频文件的封面也是AVMEDIA_TYPE_VIDEO/
            if(videoIndex != -1){
                continue;
            }
            videoIndex = i;
        }
        if (formatcontext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIndex = i;
        }
        if (formatcontext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            subtitleIndex = i;
            break;
        }
    }



    //设置解码器参数
    AVCodecParameters *videocodecPar = formatcontext->streams[videoIndex]->codecpar;
    //查找编码器
    videocodec = avcodec_find_decoder(videocodecPar->codec_id);
    //配置解码器context
    videocodeccontext = avcodec_alloc_context3(videocodec);
    //将解码器参数传到上下文
    avcodec_parameters_to_context(videocodeccontext, videocodecPar);
    // 设置多线程解码
    videocodeccontext->thread_count = 0;
    //打开解码器
    avcodec_open2(videocodeccontext, videocodec, nullptr);

    //配置音频解码器，相同操作
    AVCodecParameters *audiocodecPar = formatcontext->streams[audioIndex]->codecpar;
    audiocodec = avcodec_find_decoder(audiocodecPar->codec_id);
    audiocodeccontext = avcodec_alloc_context3(audiocodec);
    avcodec_parameters_to_context(audiocodeccontext, audiocodecPar);
    audiocodeccontext->thread_count = 0;
    avcodec_open2(audiocodeccontext, audiocodec, nullptr);





    video_time_base = formatcontext->streams[videoIndex]->time_base;
    audio_time_base = formatcontext->streams[audioIndex]->time_base;

    totaltime = formatcontext->streams[videoIndex]->duration * av_q2d(video_time_base);
    //直播的duration为0或INT64_MIN,统一设置为0
    if(totaltime <= 0){
        totaltime = 0;
    }

    //四舍五入
    fps =  qRound(av_q2d(formatcontext->streams[videoIndex]->avg_frame_rate));

    packet = av_packet_alloc();

    //获取关键帧delay方便跳转
    if(totaltime > 0){
        while(true){
            av_read_frame(formatcontext, packet);
            if(packet->stream_index == videoIndex){
                if(packet->flags == AV_PKT_FLAG_KEY){
                    if(packet->pts !=0 && keyframedelay == 0){
                        keyframedelay = packet->pts * av_q2d(video_time_base);
                        av_seek_frame(formatcontext, -1, 0, AVSEEK_FLAG_BACKWARD);
                        break;
                    }
                }
            }
        }
    }
    av_packet_unref(packet);



}



void MediaReaderThread::initfilter(){
    int video_width = videocodeccontext->width;
    int video_height = videocodeccontext->height;


    QString args = QString::asprintf("video_size=%dx%d:"
                                     "pix_fmt=%d:"
                                     "time_base=%d/%d:"
                                     "pixel_aspect=%d/%d",
                                     video_width, video_height, videocodeccontext->pix_fmt,
                                     video_time_base.num, video_time_base.den,
                                     videocodeccontext->sample_aspect_ratio.num, videocodeccontext->sample_aspect_ratio.den);


    if(subtitleIndex != -1){
        QString subtitlefilepath = filepath;

        subtitlefilepath.replace('/', "\\\\");
        subtitlefilepath.insert(subtitlefilepath.indexOf(":\\"), char('\\'));


        QString subtitlestyle = QString(
            "Fontsize=24,"
            "Outline=0.5,"
            "Shadow=1.0,"
            "BackColour=&H80000000"
            );

        QString filterDesc = QString("subtitles=filename='%1':"
                                     "original_size=%2x%3:"
                                     "force_style='%4'")
                                 .arg(subtitlefilepath).arg(video_width).arg(video_height).arg(subtitlestyle);



        const AVFilter *buffersrc = avfilter_get_by_name("buffer");
        const AVFilter *buffersink = avfilter_get_by_name("buffersink");
        AVFilterInOut *output = avfilter_inout_alloc();
        AVFilterInOut *input = avfilter_inout_alloc();
        filterGraph = avfilter_graph_alloc();

        auto release = [&output, &input] {
            avfilter_inout_free(&output);
            avfilter_inout_free(&input);
        };

        //输入过滤器
        if (avfilter_graph_create_filter(&buffersrcContext, buffersrc, "in",
                                         args.toStdString().c_str(), nullptr, filterGraph) < 0) {
            qDebug() << "create src filter"<<"Has Error: line =" << __LINE__;
            release();
            return;
        }



        if (avfilter_graph_create_filter(&buffersinkContext, buffersink, "out",
                                         nullptr, nullptr, filterGraph) < 0) {
            qDebug() << "create sink filter"<<"Has Error: line =" << __LINE__;
            release();
            return;
        }

        output->name = av_strdup("in");
        output->next = nullptr;
        output->pad_idx = 0;
        output->filter_ctx = buffersrcContext;

        input->name = av_strdup("out");
        input->next = nullptr;
        input->pad_idx = 0;
        input->filter_ctx = buffersinkContext;

        if (avfilter_graph_parse_ptr(filterGraph, filterDesc.toStdString().c_str(),
                                     &input, &output, nullptr) < 0) {
            qDebug() << "parse_ptr"<<"Has Error: line =" << __LINE__;
            release();
            return;
        }

        if (avfilter_graph_config(filterGraph, nullptr) < 0) {
            qDebug() << "config"<<"Has Error: line =" << __LINE__;
            release();
            return;
        }

        release();
    }
}




void MediaReaderThread::run(){
    emit totaltimetoVideoDecoder(totaltime);
    emit totaltimetowindow(totaltime);
    emit totaltimetoAudioDecoder(totaltime);
    emit timebasetoVideoDecoder(video_time_base);
    emit timebasetoAudioDecoder(audio_time_base);
    emit codecctxtoVideoDecoder(videocodeccontext);
    emit codecctxtoAudioDecoder(audiocodeccontext);
    emit filtercontexttoVideoDecoder(buffersrcContext, buffersinkContext);
    emit fpstoVideoDecoder(fps);
    emit keyframedelaytowindow(keyframedelay);
    while(!isInterruptionRequested()){
        if(SEEK){
            avformat_seek_file(formatcontext, -1, INT64_MIN, seekpts, INT64_MAX, AVSEEK_FLAG_BACKWARD);
            emit seekaudio();
            {
                QMutex waitmutex;
                QMutexLocker locker(&waitmutex);
                seekpress.wait(&waitmutex);
            }
            emit seekvideo();
            {
                QMutex waitmutex;
                QMutexLocker locker(&waitmutex);
                seekpress.wait(&waitmutex);
            }
            SEEK = false;
            FORCESEEND = true;
            continue;
        }
        // if(livestopwake){
        //     int seek_ret = avformat_seek_file(formatcontext, -1, INT64_MIN, 0, INT64_MAX, AVSEEK_FLAG_BACKWARD);
        //     if(seek_ret >= 0){
        //         qDebug() << "live seek success";
        //     }
        //     qDebug() << "seek ret"<<seek_ret;
        //     emit seekaudio();
        //     {
        //         QMutex waitmutex;
        //         QMutexLocker locker(&waitmutex);
        //         seekpress.wait(&waitmutex);
        //     }
        //     emit seekvideo();
        //     {
        //         QMutex waitmutex;
        //         QMutexLocker locker(&waitmutex);
        //         seekpress.wait(&waitmutex);
        //     }
        //     livestopwake = false;
        //     FORCESEEND = true;
        //     continue;
        // }
        if(STOP && !FORCESEEND){
            QMutex waitmutex;
            QMutexLocker locker(&waitmutex);
            stoppress.wait(&waitmutex);
            continue;
        }
        if(STOPREAD && !FORCESEEND){
            QMutex waitmutex;
            QMutexLocker locker(&waitmutex);
            stopread.wait(&waitmutex);
            continue;
        }
        int read_ret = av_read_frame(formatcontext, packet);
        // if(livestopwake){
        //     // qDebug() << "live stop wake";
        //     if(unrefpacketcount < 400){
        //         av_packet_unref(packet);
        //         unrefpacketcount ++;
        //         continue;
        //     }else{
        //         livestopwake = false;
        //         unrefpacketcount = 0;
        //         continue;
        //     }
        // }
        if(read_ret < 0){
            emit packettoVideoDecoder(nullptr);
            emit packettoAudioDecoder(nullptr);
        }
        if(packet->stream_index == videoIndex){
            // qDebug() << "video"<<packet->pts * av_q2d(video_time_base);
            AVPacket *videopacket = av_packet_clone(packet);
            emit packettoVideoDecoder(videopacket);
            FORCESEEND = false;
        }
        if(packet->stream_index == audioIndex){
            // qDebug() << "audio"<<packet->pts * av_q2d(audio_time_base);
            AVPacket *audiopacket = av_packet_clone(packet);
            emit packettoAudioDecoder(audiopacket);
        }
        av_packet_unref(packet);
    }
}






void MediaReaderThread::changeplaystate(){
    if(!STOP){
        STOP = true;
        qDebug() << "Reader STOP";
        // if(totaltime == 0){
        //     avformat_close_input(&formatcontext);
        //     formatcontext = nullptr;
        // }
    }else{
        if(totaltime == 0){
            // emit seekaudio();
            // emit seekvideo();
            // livestopwake = true;
            // formatcontext = avformat_alloc_context();
            // avformat_open_input(&formatcontext, filepath.toUtf8(), NULL, NULL);
            SEEK = true;
        }
        STOP = false;
        stoppress.wakeAll();
    }
}

void MediaReaderThread::seek(double seektime){
    if(!SEEK){
        double time = seektime;
        seekpts = time / av_q2d(AV_TIME_BASE_Q);
        SEEK = true;
        STOPREAD = false;
        stoppress.wakeAll();
        stopread.wakeAll();

    }
}



void MediaReaderThread::requestwake(){
    stopread.wakeAll();
    stoppress.wakeAll();
}

double MediaReaderThread::getkeyframedelay(){
    return keyframedelay;
}

void MediaReaderThread::videohadflush(){
    seekpress.wakeOne();
}

void MediaReaderThread::audiohadflush(){
    seekpress.wakeOne();
}

void MediaReaderThread::forceseend(){
    FORCESEEND = true;
    stoppress.wakeAll();
}

void MediaReaderThread::readend(){
    STOPREAD = true;
}

