#include "v4l2_video.h"
#include <QDebug>
#include <QImageReader>
#include <QBuffer>

#include <sys/mman.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

inline int clamp(int value, int min, int max)
{
    return std::max(min, std::min(value, max));
}

v4l2_buf_type type;
Vvideo::Vvideo(const bool& is_M_, QObject *parent)
    : fd(-1), is_M(is_M_)
{
    type = is_M ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

Vvideo::~Vvideo(){
    closeDevice();
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

        msleep(20); // 控制帧率，避免资源浪费
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

int Vvideo::initBuffers(){
    // 3.申请内核缓冲区
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = type;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Failed to request buffers");
        close(fd);
        return -1;
    }

    // 4.映射缓冲区到用户空间    
    std::memset(&buffer, 0, sizeof(buffer));
    buffer.type = type;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) == -1) {
        perror("Failed to query buffer");
        close(fd);
        return -1;
    }

    buffer_start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
    if (buffer_start == MAP_FAILED) {
        perror("Failed to map buffer");
        close(fd);
        return -1;
    }
    buffer_length = buffer.length;

    // 开始捕获
    if (ioctl(fd, VIDIOC_STREAMON, &buffer.type) == -1) {
        perror("Failed to start streaming");
        close(fd);
        return -1;
    }
    return 0;
}

QImage Vvideo::captureFrame()
{   
    // 5.开始采集(线程锁)
    QMutexLocker locker(&mutex); // 加锁 

    if (ioctl(fd, VIDIOC_QBUF, &buffer) == -1) {
        perror("Failed to queue buffer");
        return QImage();
    }

    if (ioctl(fd, VIDIOC_DQBUF, &buffer) == -1) {
        perror("Failed to dequeue buffer");
        return QImage();
    }
    
    // 6.数据处理
    if (fmt == V4L2_PIX_FMT_MJPEG){
        QByteArray mjpegData(static_cast<const char*>(buffer_start), buffer.bytesused);
        QImage image;
        QBuffer buffer(&mjpegData);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "JPEG");  // 指定为 JPEG 格式
        if (reader.read(&image)) {
            return image;  // 返回解码后的图像
        } else {
            qWarning() << "Failed to decode MJPEG frame!";
            return QImage();  // 返回空图像
        }
    }else if (fmt == V4L2_PIX_FMT_JPEG){
        QByteArray jpegData(static_cast<const char*>(buffer_start), buffer.bytesused);
        QImage image;
        if (!image.loadFromData(jpegData, "JPEG")) {
            qWarning() << "Failed to load JPEG image";
            return QImage();  // 如果加载失败，返回空图像
        }
        return image;  // 返回解码后的图像

    } else if (fmt == V4L2_PIX_FMT_YUYV) {
        QImage image(w, h, QImage::Format_RGB888);
        unsigned char* data = static_cast<unsigned char*>(buffer_start);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int i = (y * w + x) * 2; // 每两个字节代表一个像素（YUYV格式）
                int y_val = data[i];
                int u_val = data[i + 1];
                int v_val = data[i + 3];  // 注意：YUYV格式中的U和V分别在间隔1和3处

                int r = y_val + 1.370705 * (v_val - 128);
                int g = y_val - 0.698001 * (v_val - 128) - 0.337633 * (u_val - 128);
                int b = y_val + 1.732446 * (u_val - 128);

                // 使用clamp避免颜色溢出，确保r, g, b值在[0, 255]范围内
                image.setPixel(x, y, qRgb(clamp(r, 0, 255), clamp(g, 0, 255), clamp(b, 0, 255)));
            }
        }

        return image;  // 返回转换后的QImage
    }

    return QImage();
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
    // 7.停止采集(释放映射)
    if (ioctl(fd, VIDIOC_STREAMOFF, &buffer.type) == -1) {
        perror("Failed to start streaming");
        close(fd);
        return -1;
    }
    munmap(buffer_start, buffer_length);
    ioctl(fd, VIDIOC_STREAMOFF, type);
    // 8.关闭设备
    close(fd);
    fd = -1;
    qDebug() << "--------------";
    
    return 0;
}
