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

    struct v4l2_buffer buffer;

    QQueue<QImage> frameQueue;  // 帧队列
    void* buffer_start;        // 缓冲区起始地址
    size_t buffer_length;      // 缓冲区长度
    QMutex mutex;              // 线程锁
    __u32 w,h,fmt;
    void updateFrame();
};

#endif // V4L2_VIDEO_H