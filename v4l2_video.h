#ifndef V4L2_VIDEO_H
#define V4L2_VIDEO_H

#include <stdio.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <atomic>

#include "mywidget.h"
#include "libyuv.h"
#include "queue_.h"

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QMutexLocker>
#include <thread>
#include <QDebug>
#include <QTime>


#include <linux/videodev2.h>

using namespace std;

#define MAX_PLANES 3  // 假设最多支持3个平面

typedef struct __frame {
    void *start;                // 存储每个平面映射的内存
    size_t length;               // 每个平面的长度
    bool in_use;                 // 是否正在使用
} frame_data;

typedef struct __video_buffer {
    frame_data fm[MAX_PLANES];
    int plane_count;            // 平面的数量
} video_buf_t;

struct TimedImage {
    QImage image;
    QTime timestamp;  // 记录入队的时间戳
};

class Vvideo : public QObject {
    Q_OBJECT    // 信号与槽必要宏
public:
    
    explicit Vvideo(const bool& is_M_, MyWidget *Label, QObject *parent=nullptr);
    ~Vvideo();

    void run() {
        // 开启线程
        captureThread_ = std::thread(&Vvideo::captureFrame, this);
        processThread_ = std::thread(&Vvideo::processFrame, this, displayLabel);

        captureThread_.detach();
        processThread_.detach();
        qDebug()<<"Thread running...";
        while (!quit_) {
            // 执行线程的主要逻辑
            std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // 模拟工作
        }
        qDebug()<<"Thread exited.";
    }
    
    int openDevice(const QString& deviceName);
    
    int setFormat(const __u32& w_, const __u32&h_, const __u32& fmt_);
    int initBuffers();

    void updateImage();
    void takePic(QImage &img);
    int closeDevice();
  
    void stop() {
        quit_ = true;  // 设置退出标志
    }  
private:
    int fd;
    bool is_M;
    __u32 w,h,fmt;
    std::atomic<bool> quit_{false};  // 使用 atomic 防止竞态 退出标志
    // QMutex mutex;              /* 线程锁交由queue处理 */
    std::thread captureThread_;
    std::thread processThread_;
    MyWidget *displayLabel = nullptr;
    SafeQueue<TimedImage> frameQueue; // 测试
    SafeQueue<int> frameIndexQueue; // 尝试用索引队列(失败)
    // SafeQueue<QImage> QImageframes;    // 处理后帧队列
    
    struct v4l2_buffer buffer;
    video_buf_t *framebuf = nullptr; // 映射
    
    int captureFrame();
    void processFrame(MyWidget *displayLabel);

    int initSinglePlaneBuffers();
    int initMultiPlaneBuffers();

    bool MJPG2RGB(QImage &image_, void *data, size_t length);
    bool YUYV2RGB(QImage &image_, void *data, size_t length);
    bool NV12ToRGB(QImage &image_, void *data_y, size_t len_y, void *data_uv, size_t len_uv);

};

#endif // V4L2_VIDEO_H