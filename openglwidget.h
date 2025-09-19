#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMutex>
#include <QApplication>
#include <QQueue>
#include <QScreen>
#include <QKeyEvent>
#include <windows.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}


class openglWidget : public QOpenGLWidget,  protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit openglWidget(QWidget *parent = nullptr);
    ~openglWidget();
    void clear(); // 新增：清除当前帧的函数
public slots:
    void updateFrame(AVFrame *frame);
protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    //opengl
    QOpenGLShaderProgram *shaderProgram = nullptr;
    QOpenGLBuffer vbo;
    QOpenGLVertexArrayObject vao;
    // YUV纹理
    GLuint textureY = 0;
    GLuint textureU = 0;
    GLuint textureV = 0;

    //frame
    QMutex frameMutex;
    AVFrame *bufferframe = nullptr;
    AVFrame *presentframe = nullptr;
    // 视频帧尺寸
    int videoWidth = 0;
    int videoHeight = 0;


    //screen
    int screenw;
    int screenh;
    QRect origingeometry;




    void initTextures();
    void uploadYUVTextures(AVFrame *frame);
    void checkGLError(const char* context);
    void cleanupGL();
};

#endif // OPENGLWIDGET_H
