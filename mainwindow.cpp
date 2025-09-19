#include "mainwindow.h"
#include "./ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    videowindow = ui->openGLWidget;


    // // 初始化进度条
    ui->horizontalSlider_videobar->setRange(0, 100);
    ui->horizontalSlider_videobar->setValue(0);

    videowindowgeometry = videowindow->geometry();


}

MainWindow::~MainWindow()
{
    stopCurrentVideo();
    delete ui;
}

void MainWindow::stopCurrentVideo()
{
    on_pushButton_play_clicked();
    if (MediaReader) {
        // 断开所有信号连接
        disconnect(MediaReader, nullptr, this, nullptr);
        disconnect(MediaReader, nullptr, VideoDecoder, nullptr);
        disconnect(MediaReader, nullptr, AudioDecoder, nullptr);


        //停止并删除解码器
        MediaReader->requestInterruption();
        MediaReader->requestwake();
        // MediaReader->wait();
        MediaReader->deleteLater();
        MediaReader = nullptr;
    }
    if(AudioDecoder){
        disconnect(AudioDecoder, nullptr, VideoDecoder, nullptr);
        disconnect(AudioDecoder, nullptr, MediaReader, nullptr);
        //停止并删除解码器
        AudioDecoder->requestInterruption();
        AudioDecoder->requestwake();
        AudioDecoder->wait();
        AudioDecoder->deleteLater();
        AudioDecoder = nullptr;
    }
    if(VideoDecoder){
        disconnect(VideoDecoder, nullptr, videowindow, nullptr);
        disconnect(VideoDecoder, nullptr, this, nullptr);
        disconnect(VideoDecoder, nullptr, MediaReader, nullptr);
        disconnect(VideoDecoder, nullptr, AudioDecoder, nullptr);

        VideoDecoder->requestInterruption();
        VideoDecoder->requestwake();
        VideoDecoder->wait();
        VideoDecoder->deleteLater();
        VideoDecoder = nullptr;
    }

    // 清除视频窗口内容
    if (videowindow) {
        videowindow->clear();
    }
}


void MainWindow::on_action_local_triggered(){
    //每次打开文件停止正在播放的视频
    stopCurrentVideo();
    ui->horizontalSlider_videobar->setValue(0);
    QTime displaytime(0,0,0);
    ui->label_currenttime->setText(displaytime.toString("hh:mm:ss"));
    ui->label_totaltime->setText(displaytime.toString("hh:mm:ss"));
    //打开一个dialog读取视频位置
    QString filepath = QFileDialog::getOpenFileName(this, "打开文件",QString(),"*.mp4 *.mkv *.avi *.mov");
    if(filepath == ""){
        return;
    }

    decodeandplay(filepath);
}

void MainWindow::on_action_URL_triggered(){
    stopCurrentVideo();
    ui->horizontalSlider_videobar->setValue(0);
    QTime displaytime(0,0,0);
    ui->label_currenttime->setText(displaytime.toString("hh:mm:ss"));
    ui->label_totaltime->setText(displaytime.toString("hh:mm:ss"));
    QString URLpath = QInputDialog::getMultiLineText(this, "打开URL", "输入URL地址");

    if(URLpath == ""){
        return;
    }
    decodeandplay(URLpath);
}




void MainWindow::settotaltime(double time){
    totaltime = time;
    QTime videototaltime(0,0,0);
    int timems = totaltime * 1000;
    videototaltime = videototaltime.addMSecs(timems);
    ui->label_totaltime->setText(videototaltime.toString("hh:mm:ss"));
}

void MainWindow::updateslider(double time){
    //显示当前时间
    {
        QMutexLocker locker(&timemutex);
        timenow = time;
    }
    QTime displaytime(0,0,0);
    int timems = timenow * 1000;
    displaytime = displaytime.addMSecs(timems);
    ui->label_currenttime->setText(displaytime.toString("hh:mm:ss"));

    if(!VIDEODRAG){
        //更新进度条
        int sliderValue = (time * 100) / totaltime;
        ui->horizontalSlider_videobar->setValue(sliderValue);
    }
}



void MainWindow::on_pushButton_play_clicked()
{
    if(!MediaReader){
        return;
    }
    MediaReader->changeplaystate();
    VideoDecoder->changeplaystate();
    AudioDecoder->changeplaystate();
}

void MainWindow::on_pushButton_backforward_clicked()
{
    QMutexLocker locker(&seekmutex);
    if(!MediaReader){
        return;
    }
    if(totaltime == 0){
        return;
    }
    if(SEEKING){
        return;
    }
    SEEKING = true;
    double seektimevalue = -5.0;
    if(keyframedelay > fabs(seektimevalue)){
        seektimevalue = -keyframedelay;
    }
    double seektime = timenow + seektimevalue;
    MediaReader->seek(seektime);
    VideoDecoder->dequeuewake();
}

void MainWindow::on_pushButton_seekforward_clicked()
{
    QMutexLocker locker(&seekmutex);
    if(!MediaReader){
        return;
    }
    if(totaltime == 0){
        return;
    }
    if(SEEKING){
        return;
    }
    SEEKING = true;
    double seektimevalue = 5.0;
    if(keyframedelay > seektimevalue){
        seektimevalue = keyframedelay + 1;
    }
    double seektime = timenow + seektimevalue;
    MediaReader->seek(seektime);
    VideoDecoder->dequeuewake();
}



void MainWindow::on_horizontalSlider_videobar_sliderPressed()
{
    QMutexLocker locker(&seekmutex);
    if(!MediaReader){
        return;
    }
    if(totaltime == 0){
        return;
    }
    if(SEEKING){
        return;
    }
    SEEKING = true;
    double seektime = (double) ui->horizontalSlider_videobar->value() / 100 * totaltime;

    MediaReader->seek(seektime);
    VideoDecoder->dequeuewake();
}



void MainWindow::on_horizontalSlider_videobar_sliderMoved(int position)
{
    QMutexLocker locker(&seekmutex);
    if(!MediaReader){
        return;
    }
    if(totaltime == 0){
        return;
    }
    VIDEODRAG = true;
    if(SEEKING){
        return;
    }

    SEEKING = true;
    double seektime = (double) position / 100 * totaltime;

    MediaReader->seek(seektime);
    VideoDecoder->dequeuewake();
}

void MainWindow::on_horizontalSlider_videobar_sliderReleased()
{
    VIDEODRAG = false;
}





void MainWindow::decodeandplay(QString path){
    //打开解码线程
    MediaReader = new MediaReaderThread(path);

    AudioDecoder = new AudioDecoderThread();


    VideoDecoder = new VideoDecoderThread;


    MediaReader->start();
    VideoDecoder->start();
    AudioDecoder->start();


    //MediaReader
    connect(MediaReader, &MediaReaderThread::timebasetoVideoDecoder, VideoDecoder, &VideoDecoderThread::settimebase, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::totaltimetoVideoDecoder, VideoDecoder, &VideoDecoderThread::settotaltime, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::codecctxtoAudioDecoder, AudioDecoder, &AudioDecoderThread::getcodecctx, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::packettoAudioDecoder, AudioDecoder, &AudioDecoderThread::updatpacket, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::codecctxtoVideoDecoder, VideoDecoder, &VideoDecoderThread::setcodecctx, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::packettoVideoDecoder, VideoDecoder, &VideoDecoderThread::updatpacket, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::totaltimetowindow, this, &MainWindow::settotaltime, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::seekvideo, VideoDecoder, &VideoDecoderThread::seekframe, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::seekaudio, AudioDecoder, &AudioDecoderThread::seekframe, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::filtercontexttoVideoDecoder, VideoDecoder, &VideoDecoderThread::setfiltercontext, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::fpstoVideoDecoder, VideoDecoder, &VideoDecoderThread::setqueuesize, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::keyframedelaytowindow, this, &MainWindow::setkeyframedelay, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::timebasetoAudioDecoder, AudioDecoder, &AudioDecoderThread::settimebase, Qt::DirectConnection);
    connect(MediaReader, &MediaReaderThread::totaltimetoAudioDecoder, AudioDecoder, &AudioDecoderThread::settotaltime, Qt::DirectConnection);

    //VideoDecoder
    connect(VideoDecoder, &VideoDecoderThread::frametogl, videowindow, &openglWidget::updateFrame, Qt::DirectConnection);
    connect(VideoDecoder, &VideoDecoderThread::timetowindow, this, &MainWindow::updateslider, Qt::DirectConnection);
    connect(VideoDecoder, &VideoDecoderThread::seeksuccess, this, &MainWindow::canseek, Qt::DirectConnection);
    connect(VideoDecoder, &VideoDecoderThread::videoflush, MediaReader, &MediaReaderThread::videohadflush, Qt::DirectConnection);
    connect(VideoDecoder, &VideoDecoderThread::needpacket, MediaReader, &MediaReaderThread::forceseend, Qt::DirectConnection);
    connect(VideoDecoder, &VideoDecoderThread::seektimetoclock, AudioDecoder, &AudioDecoderThread::setseektime, Qt::DirectConnection);
    connect(VideoDecoder, &VideoDecoderThread::decodeendtoaudio, AudioDecoder, &AudioDecoderThread::audioend, Qt::DirectConnection);
    connect(VideoDecoder, &VideoDecoderThread::decodeendtoreader, MediaReader, &MediaReaderThread::readend, Qt::DirectConnection);

    //AudioDecoder
    connect(AudioDecoder, &AudioDecoderThread::clocktoVideoDecoder, VideoDecoder, &VideoDecoderThread::updateAudioClock, Qt::DirectConnection);
    connect(AudioDecoder, &AudioDecoderThread::starttimetoVideoDecoder, VideoDecoder, &VideoDecoderThread::setstarttime, Qt::DirectConnection);
    connect(AudioDecoder, &AudioDecoderThread::audioflush, MediaReader, &MediaReaderThread::audiohadflush, Qt::DirectConnection);
}


void MainWindow::on_pushButton_fullscreen_clicked()
{
    videowindow->setWindowFlags(Qt::Window);
    videowindow->showFullScreen();
    //不息屏隐藏鼠标
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    QApplication::setOverrideCursor(Qt::BlankCursor);
    this->hide();

}

QRect MainWindow::getvideowindowgeometry(){
    return videowindowgeometry;
}


void MainWindow::canseek(){
    QMutexLocker locker(&seekmutex);
    SEEKING = false;
}

void MainWindow::setkeyframedelay(double delay){
    keyframedelay = delay;
}


