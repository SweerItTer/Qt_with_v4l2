#include "v4l2_video.h"
#include <QDebug>
#include <QImageReader>
#include <QBuffer>

#include <sys/mman.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

#define BUFCOUNT 8
inline int clamp(int value, int min, int max)
{
    return std::max(min, std::min(value, max));
}

v4l2_buf_type type;
Vvideo::Vvideo(const bool& is_M_, QObject *parent)
    : fd(-1), is_M(is_M_)
{
    type = is_M ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    framebuf = new video_buf_t[BUFCOUNT];
}

Vvideo::~Vvideo(){
    closeDevice();

    // 释放缓冲区
    if (framebuf != nullptr) {
        delete[] framebuf;
        framebuf = nullptr;
    }
}

void Vvideo::run() {
    // 进入线程主循环
    while (!quit_) {
        QImage frame = captureFrame();  // 捕获一帧图像

        if (!frame.isNull()) {
            QMutexLocker locker(&mutex);  // 锁定队列
            frameQueue.enqueue(frame);    // 将帧添加到队列
        } else {
            // 捕获失败，可以选择继续循环或退出
            qWarning() << "Capture frame failed!";
        }

        msleep(15); // 控制帧率，避免资源浪费
        updateFrame();  // 每次捕获后调用 updateFrame 以处理队列中的数据
    }
}

int Vvideo::openDevice(const QString& deviceName)
{
    // 1.打开设备
    fd = open(deviceName.toLocal8Bit().constData(), O_RDWR);
    if (fd < 0) {
        // 打开设备失败
        qDebug() << "Failed to open device" << deviceName;
        return -1;
    }

    return 0;
}

int Vvideo::setFormat(const __u32 &w_, const __u32 &h_, const __u32 &fmt_)
{   
    struct v4l2_format format;
    std::memset(&format, 0, sizeof(format));

    // 2.配置设备
    format.type = type;

    format.fmt.pix.width = w_;
    format.fmt.pix.height = h_;
    format.fmt.pix.pixelformat = fmt_;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &format) == -1) {
        perror("Failed to set video format");
        close(fd);
        return -1;
    }
    w = w_;
    h = h_;
    fmt = fmt_;
    qDebug() <<w <<h <<fmt;
    return 0;
}

int Vvideo::initBuffers() {
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = BUFCOUNT;
    req.type = type;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Failed to request buffers");
        close(fd);
        return -1;
    }

    for (int num = 0; num < BUFCOUNT; num++) {
        std::memset(&buffer, 0, sizeof(buffer));
        buffer.type = type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = num;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) == -1) {
            perror("Failed to query buffer");
            goto cleanup;
        }

        framebuf[num].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
        if (framebuf[num].start == MAP_FAILED) {
            perror("Failed to map buffer");
            goto cleanup;
        }
        framebuf[num].length = buffer.length;

        if (ioctl(fd, VIDIOC_QBUF, &buffer) == -1) {
            perror("Failed to queue buffer");
            goto cleanup;
        }
    }

    if (ioctl(fd, VIDIOC_STREAMON, &buffer.type) == -1) {
        perror("Failed to start streaming");
        goto cleanup;
    }

    return 0;

cleanup:
    for (int i = 0; i < BUFCOUNT; i++) {
        if (framebuf[i].start && framebuf[i].start != MAP_FAILED) {
            munmap(framebuf[i].start, framebuf[i].length);
        }
    }
    close(fd);
    return -1;
}

QImage Vvideo::captureFrame() {
    QMutexLocker locker(&mutex); // 加锁

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = type;
    buffer.memory = V4L2_MEMORY_MMAP;

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    int r = select(fd + 1, &fds, NULL, NULL, &tv);
    if (r == -1) {
        perror("select");
        return QImage();
    } else if (r == 0) {
        qDebug() << "Timeout waiting for buffer";
        return QImage();
    }

    if (ioctl(fd, VIDIOC_DQBUF, &buffer) == -1) {
        perror("Failed to dequeue buffer");
        return QImage();
    }

    QImage image_;
    if (fmt == V4L2_PIX_FMT_MJPEG || fmt == V4L2_PIX_FMT_JPEG) {
        MJPG2RGB(image_, framebuf[buffer.index].start, framebuf[buffer.index].length);
    } else if (fmt == V4L2_PIX_FMT_YUYV) {
        YUYV2RGB(image_, framebuf[buffer.index].start, framebuf[buffer.index].length);
    } else {
        qDebug() << "Unsupported format";
    }

    if (ioctl(fd, VIDIOC_QBUF, &buffer) == -1) {
        perror("Failed to queue buffer");
    }
    return image_;
}

void Vvideo::YUYV2RGB(QImage &image_, void *data, size_t length) {
    QImage image(w, h, QImage::Format_RGB888);
    unsigned char* data_ = static_cast<unsigned char*>(data);

    // 遍历每个像素
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int i = (y * w + x) * 2; // 每两个字节代表一个像素（YUYV格式）

            // YUYV格式：Y, U, Y, V
            int y_val = data_[i];          // Y值
            int u_val = data_[i + 1];      // U值
            int v_val = data_[i + 2];      // V值（修正索引）

            // YUV到RGB的转换公式
            int r = y_val + 1.370705 * (v_val - 128);
            int g = y_val - 0.698001 * (v_val - 128) - 0.337633 * (u_val - 128);
            int b = y_val + 1.732446 * (u_val - 128);

            // 使用clamp避免颜色溢出，确保r, g, b值在[0, 255]范围内
            r = clamp(r, 0, 255);
            g = clamp(g, 0, 255);
            b = clamp(b, 0, 255);

            // 使用qRgb将RGB值设置到图像像素
            image.setPixel(x, y, qRgb(r, g, b));
        }
    }

    image_ = image;  // 返回转换后的QImage
}

void Vvideo::MJPG2RGB(QImage &image_, void *data, size_t length){
    QByteArray mjpegData(static_cast<const char*>(data), length);
    QImage image;
    QBuffer buffer(&mjpegData);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "JPEG");  // 指定为 JPEG 格式
    if (reader.read(&image)) {
        image_ = image;  // 返回解码后的图像
    } else {
        qWarning() << "Failed to decode MJPEG frame!";
        return;  // 返回空图像
    }
}

void Vvideo::updateFrame() {
    QMutexLocker locker(&mutex);  // 锁定队列
    if (!frameQueue.isEmpty()) {
        QImage frame = frameQueue.dequeue();  // 从队列中取出一帧
        emit frameAvailable(frame);  // 发射信号，通知主界面更新显示
    }
}

int Vvideo::closeDevice()
{
    frameQueue.clear();
    // 7.停止采集(释放映射)
    if (ioctl(fd, VIDIOC_STREAMOFF, &buffer.type) == -1) {
        perror("Failed to start streaming");
        close(fd);
        return -1;
    }
    for (int i = 0; i <= BUFCOUNT; i++) {
        if (framebuf[i].start) {
            munmap(framebuf[i].start, framebuf[i].length);
        }
    }
    ioctl(fd, VIDIOC_STREAMOFF, type);
    // 8.关闭设备
    close(fd);
    fd = -1;
    qDebug() << "--------------";
    
    return 0;
}
