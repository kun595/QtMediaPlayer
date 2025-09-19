#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QScreen>
#include <QInputDialog>
#include <windows.h>
#include "MediaReaderThread.h"
#include "openglwidget.h"
#include "AudioDecoderThread.h"
#include "VideoDecoderThread.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QRect getvideowindowgeometry();

public slots:
    void updateslider(double time);
    void on_pushButton_backforward_clicked();

    void on_pushButton_seekforward_clicked();
    void on_pushButton_play_clicked();
    void settotaltime(double time);
    void canseek();
    void setkeyframedelay(double delay);
private slots:
    void on_action_local_triggered();

    void on_action_URL_triggered();



    void on_horizontalSlider_videobar_sliderReleased();

    void on_horizontalSlider_videobar_sliderPressed();

    void on_horizontalSlider_videobar_sliderMoved(int position);

    void on_pushButton_fullscreen_clicked();
private:
    //object
    Ui::MainWindow *ui;
    openglWidget *videowindow;
    MediaReaderThread *MediaReader = nullptr;
    AudioDecoderThread *AudioDecoder = nullptr;
    VideoDecoderThread *VideoDecoder = nullptr;

    //time
    QMutex timemutex;
    double timenow = 0.0;
    double totaltime;

    //seek
    QMutex seekmutex;
    bool SEEKING = false;
    double keyframedelay;
    bool VIDEODRAG = false;

    //opengl
    QRect videowindowgeometry;


    void stopCurrentVideo();
    void showtotaltime();
    void decodeandplay(QString path);
};
#endif // MAINWINDOW_H
