#include "openglwidget.h"
#include "mainwindow.h"


openglWidget::openglWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // 设置OpenGL版本和核心模式
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
}

openglWidget::~openglWidget()
{
    makeCurrent();

    // 释放OpenGL资源
    if (shaderProgram) {
        shaderProgram->release();
        delete shaderProgram;
        shaderProgram = nullptr;
    }

    if (vbo.isCreated()) vbo.destroy();
    if (vao.isCreated()) vao.destroy();

    // 删除纹理
    glDeleteTextures(1, &textureY);
    glDeleteTextures(1, &textureU);
    glDeleteTextures(1, &textureV);

    doneCurrent();
}
void openglWidget::checkGLError(const char* context)
{
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        qDebug() << "OpenGL error @" << context << ": 0x" << Qt::hex << err;
    }
}


void openglWidget::initializeGL()
{
    initializeOpenGLFunctions();
    // 打印OpenGL版本信息
    qDebug() << "OpenGL Version:" << reinterpret_cast<const char*>(glGetString(GL_VERSION));
    qDebug() << "GLSL Version:" << reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    qDebug() << "Vendor:" << reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    qDebug() << "Renderer:" << reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    videoWidth = 0;
    videoHeight = 0;

    //每次resize全屏都会重新调用initializeGL,重新初始化
    //所以需要清理先前的资源
    cleanupGL();
    // 设置黑色背景
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // 创建着色器程序
    shaderProgram = new QOpenGLShaderProgram(this);

    // 从文件加载着色器
    QString sourceDir = QFileInfo(__FILE__).absolutePath();
    shaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex,sourceDir + "/shaders/vertexshader.vert");
    shaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment,sourceDir + "/shaders/fragmentshader.frag");

    shaderProgram->link();

    if (!shaderProgram->isLinked()) {
        qDebug() << "Shader linking error:" << shaderProgram->log();
    }

    checkGLError("After shaders");

    GLfloat vertices[] = {
        // 位置       // 纹理坐标
        -1.0f, -1.0f, 0.0f, 1.0f, // 左下
        1.0f, -1.0f, 1.0f, 1.0f, // 右下
        -1.0f,  1.0f, 0.0f, 0.0f, // 左上
        1.0f,  1.0f, 1.0f, 0.0f  // 右上
    };
    // 创建VAO，VBO
    vao.create();
    vbo.create();

    //绑定VAO,VBO
    vao.bind();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    checkGLError("After VAO/VBO setup");
    // 绑定着色器
    shaderProgram->bind();

    // 位置属性
    shaderProgram->enableAttributeArray(0);
    shaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));


    // 纹理坐标属性
    shaderProgram->enableAttributeArray(1);
    shaderProgram->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));


    // 设置着色器纹理单元
    shaderProgram->setUniformValue("textureY", 0);
    shaderProgram->setUniformValue("textureU", 1);
    shaderProgram->setUniformValue("textureV", 2);

    checkGLError("After setUniformValue");
    // 初始化纹理
    initTextures();

    shaderProgram->release();
    vbo.release();
    vao.release();
}




void openglWidget::initTextures(){
    // 创建YUV三个纹理
    glGenTextures(1, &textureY);
    glGenTextures(1, &textureU);
    glGenTextures(1, &textureV);


    checkGLError("After texture generation");

    // 配置纹理参数
    auto configureTexture = [this](GLuint texture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };

    configureTexture(textureY);
    configureTexture(textureU);
    configureTexture(textureV);


    checkGLError("After texture configuration");
}


void openglWidget::uploadYUVTextures(AVFrame *frame){
    // 检查尺寸是否变化
    bool sizeChanged = (videoWidth != frame->width || videoHeight != frame->height);

    // 更新当前尺寸
    videoWidth = frame->width;
    videoHeight = frame->height;

    // 获取行对齐信息
    int yLinesize = frame->linesize[0];
    int uLinesize = frame->linesize[1];
    int vLinesize = frame->linesize[2];

    // Y分量
    glBindTexture(GL_TEXTURE_2D, textureY);
    if (sizeChanged) {
        // 尺寸变化时重新分配内存
        glPixelStorei(GL_UNPACK_ROW_LENGTH, yLinesize);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth, videoHeight, 0,
                     GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);
    } else {
        // 尺寸未变化时更新纹理数据
        glPixelStorei(GL_UNPACK_ROW_LENGTH, yLinesize);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoWidth, videoHeight,
                        GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);
    }

    // U分量
    glBindTexture(GL_TEXTURE_2D, textureU);
    if (sizeChanged) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, uLinesize);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth/2, videoHeight/2, 0,
                     GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);
    } else {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, uLinesize);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoWidth/2, videoHeight/2,
                        GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);
    }

    // V分量
    glBindTexture(GL_TEXTURE_2D, textureV);
    if (sizeChanged) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, vLinesize);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth/2, videoHeight/2, 0,
                     GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);
    } else {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, vLinesize);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoWidth/2, videoHeight/2,
                        GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

}



void openglWidget::clear()
{
    if(presentframe){
        av_frame_free(&presentframe);
    }
    if(bufferframe){
        av_frame_free(&bufferframe);
    }
    update(); // 触发重绘（将显示黑色背景）
}

void openglWidget::updateFrame(AVFrame *frame){
    QMutexLocker locker(&frameMutex);
    if(bufferframe){
        av_frame_free(&bufferframe);
        bufferframe = nullptr;
    }
    bufferframe = av_frame_clone(frame);

    update(); // 同步重绘
}





void openglWidget::paintGL()
{

    QMutexLocker locker(&frameMutex);

    glClear(GL_COLOR_BUFFER_BIT);

    //双缓冲机制确保内存释放
    if(bufferframe){
        if(presentframe){
            av_frame_free(&presentframe);
            presentframe = nullptr;
        }
        presentframe = bufferframe;
        bufferframe = nullptr;
    }

    if (presentframe) {

        shaderProgram->bind();

        // 更新纹理
        uploadYUVTextures(presentframe);

        // 绑定纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureY);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureU);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textureV);

        // 绘制
        vao.bind();
        double ratio = (double)videoHeight / videoWidth;
        glViewport(0, (screenh-screenw * ratio)/2.0, screenw, screenw * ratio);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        vao.release();

        // 解绑
        shaderProgram->release();

    }


}

void openglWidget::resizeGL(int w, int h)
{
    QScreen *screen = this->screen();

    qreal dpr = screen->devicePixelRatio();

    screenw = w * dpr;
    screenh = h * dpr;
    qDebug() << "Resized to:" << w << "x" << h;
}

void openglWidget::cleanupGL(){
    if(vao.isCreated()){
        vao.destroy();
    }
    if(vbo.isCreated()){
        vbo.destroy();
    }
    if(shaderProgram){
        delete shaderProgram;
    }
    if(glIsTexture(textureY)){
        glDeleteTextures(1, &textureY);
    }
    if(glIsTexture(textureU)){
        glDeleteTextures(1, &textureU);
    }
    if(glIsTexture(textureV)){
        glDeleteTextures(1, &textureV);
    }
}

void openglWidget::keyPressEvent(QKeyEvent *event){
//parent是centralwidget,parent->parent才是mainwindow
    MainWindow* mainwin = qobject_cast<MainWindow*>(this->parent()->parent());

    if(event->key() == Qt::Key_Escape){
        this->setWindowFlag(Qt::Window, false);
        this->showNormal();
        this->setGeometry(mainwin->getvideowindowgeometry());
        SetThreadExecutionState(ES_CONTINUOUS);
        QApplication::setOverrideCursor(Qt::ArrowCursor);
        mainwin->show();
    }
    if(event->key() == Qt::Key_Right){
        mainwin->on_pushButton_seekforward_clicked();
    }
    if(event->key() == Qt::Key_Left){
        mainwin->on_pushButton_backforward_clicked();
    }
    if(event->key() == Qt::Key_Space){
        mainwin->on_pushButton_play_clicked();
    }
}


