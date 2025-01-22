#ifndef V4L2_VIDEO_H
#define V4L2_VIDEO_H

#include <stdio.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <QObject>
#include <QImage>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

#include <linux/videodev2.h>

using namespace std;

// typedef struct __video_buffer {
//     void *start;
//     size_t length;
// } video_buf_t;

#define MAX_PLANES 3  // 假设最多支持3个平面

typedef struct __video_buffer {
    void *start[MAX_PLANES];  // 存储每个平面映射的内存
    size_t length[MAX_PLANES]; // 每个平面的长度
    int plane_count;           // 平面的数量
} video_buf_t;

class Vvideo : public QThread 
{
    Q_OBJECT
public:
    
    explicit Vvideo(const bool& is_M_, QObject *parent = nullptr);
    ~Vvideo();

    void run() override ;
    int quit_ = 0;

    int openDevice(const QString& deviceName);
    
    int setFormat(const __u32& w_, const __u32&h_, const __u32& fmt_);
    int initBuffers();
    QImage captureFrame();

    int closeDevice();

signals:
    void frameAvailable(const QImage &frame);
    
private:
    int fd;
    bool is_M;

    QQueue<QImage> frameQueue;  // 帧队列
    QMutex mutex;              // 线程锁
    __u32 w,h,fmt;

    struct v4l2_buffer buffer;
    video_buf_t *framebuf = nullptr;
    
    void updateFrame();

    int initSinglePlaneBuffers();
    int initMultiPlaneBuffers();

    void MJPG2RGB(QImage &image_, void *data, size_t length);
    void YUYV2RGB(QImage &image_, void *data, size_t length);
    void NV12ToRGB(QImage &image_, void *data_y, size_t len_y, void *data_uv, size_t len_uv);

};

#endif // V4L2_VIDEO_H