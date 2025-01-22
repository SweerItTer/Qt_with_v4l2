#include "v4l2_video.h"
#include <QDebug>
#include <QImageReader>
#include <QBuffer>

#include <sys/mman.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

#include <turbojpeg.h>

#define BUFCOUNT 40
#define FMT_NUM_PLANES 2

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
    QImage frame;
    // 进入线程主循环
    while (!quit_) {
        frame = captureFrame();  // 捕获一帧图像

        if (!frame.isNull()) {
            QMutexLocker locker(&mutex);  // 锁定队列
            if( frameQueue.length() <= 40 )
                frameQueue.enqueue(frame);    // 将帧添加到队列
            else continue;
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

    struct v4l2_streamparm streamparm;
    std::memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = type;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = 30; // 30FPS

    if (ioctl(fd, VIDIOC_S_PARM, &streamparm) < 0) {
        perror("Failed to set frame rate");
    }


    // 验证帧率设置
    if (ioctl(fd, VIDIOC_G_PARM, &streamparm) == 0) {
        printf("Frame rate is now %d/%d FPS\n",
            streamparm.parm.capture.timeperframe.numerator,
            streamparm.parm.capture.timeperframe.denominator);
    }
    return 0;
}

int Vvideo::initBuffers() {
    if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return initSinglePlaneBuffers();
    } else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
        return initMultiPlaneBuffers();
    } else {
        perror("Unsupported buffer type");
        return -1;
    }
}
// 单面
int Vvideo::initSinglePlaneBuffers(){
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

        framebuf[num].start[0] = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
        if (framebuf[num].start == MAP_FAILED) {
            perror("Failed to map buffer");
            goto cleanup;
        }
        framebuf[num].length[0] = buffer.length;

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
            munmap(framebuf[i].start, framebuf[i].length[0]);
        }
    }
    close(fd);
    return -1;
}
// 多面
int Vvideo::initMultiPlaneBuffers() {
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = BUFCOUNT;
    req.type = type;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("Failed to request buffers");
        close(fd);
        return -1;
    }

    for (int num = 0; num < req.count; num++) {
        struct v4l2_plane planes[FMT_NUM_PLANES];
        std::memset(&planes, 0, sizeof(planes));
        std::memset(&buffer, 0, sizeof(buffer));

        buffer.type = type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = num;
        buffer.length = FMT_NUM_PLANES;
        buffer.m.planes = planes;

        // 查询缓冲区
        if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) == -1) {
            perror("Failed to query buffer");
            goto cleanup;
        }

        framebuf[num].plane_count = buffer.length;  // 实际平面数量

        for (int plane = 0; plane < framebuf[num].plane_count; plane++) {
            framebuf[num].length[plane] = buffer.m.planes[plane].length;
            framebuf[num].start[plane] = mmap(
                NULL, framebuf[num].length[plane], PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, buffer.m.planes[plane].m.mem_offset);

            if (framebuf[num].start[plane] == MAP_FAILED) {
                perror("Failed to map plane buffer");
                for (int j = 0; j < plane; j++) {
                    if (framebuf[num].start[j] != MAP_FAILED) {
                        munmap(framebuf[num].start[j], framebuf[num].length[j]);
                        framebuf[num].start[j] = nullptr;
                    }
                }
                goto cleanup;
            }
        }
    }

    // 将所有缓冲区加入队列
    for (int num = 0; num < req.count; num++) {
        struct v4l2_plane planes[FMT_NUM_PLANES];
        std::memset(&planes, 0, sizeof(planes));
        std::memset(&buffer, 0, sizeof(buffer));

        buffer.type = type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = num;
        buffer.m.planes = planes;
        buffer.length = FMT_NUM_PLANES;

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
    for (int i = 0; i < req.count; i++) {
        for (int plane = 0; plane < framebuf[i].plane_count; plane++) {
            if (framebuf[i].start[plane] && framebuf[i].start[plane] != MAP_FAILED) {
                munmap(framebuf[i].start[plane], framebuf[i].length[plane]);
            }
        }
    }
    close(fd);
    return -1;
}
QImage Vvideo::captureFrame() {
    struct v4l2_plane planes[FMT_NUM_PLANES];
    memset(planes, 0, sizeof(planes));
    memset(&buffer, 0, sizeof(buffer));
    buffer.type = type;
    buffer.memory = V4L2_MEMORY_MMAP;

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == type) {
        buffer.m.planes = planes;
        buffer.length = FMT_NUM_PLANES;
    }
    QMutexLocker locker(&mutex); // 加锁

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
    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == type) {
        for (int plane = 0; plane < buffer.length; plane++) {
            if (planes[plane].bytesused == 0) continue;

            if (fmt == V4L2_PIX_FMT_NV12) {
                NV12ToRGB(image_, framebuf[buffer.index].start[0], planes[0].bytesused,
                          framebuf[buffer.index].start[1], planes[1].bytesused);
            } else if (fmt == V4L2_PIX_FMT_MJPEG || fmt == V4L2_PIX_FMT_JPEG) {
                MJPG2RGB(image_, framebuf[buffer.index].start[plane], planes[plane].bytesused);
            } else if (fmt == V4L2_PIX_FMT_YUYV) {
                YUYV2RGB(image_, framebuf[buffer.index].start[plane], planes[plane].bytesused);
            } else {
                qDebug() << "Unsupported format";
            }
        }
    } else {
        MJPG2RGB(image_, framebuf[buffer.index].start[0], framebuf[buffer.index].length[0]);
    }

    if (ioctl(fd, VIDIOC_QBUF, &buffer) == -1) {
        perror("Failed to queue buffer");
    }
    return image_;
}

void Vvideo::YUYV2RGB(QImage &image_, void *data, size_t length) {
    QImage image(w, h, QImage::Format_RGB888);
    unsigned char* data_ = static_cast<unsigned char*>(data);

    for (int y = 0; y < h; ++y) {
        unsigned char* line = image.scanLine(y); // 获取当前行
        for (int x = 0; x < w; x += 2) {
            int i = (y * w + x) * 2; // 每4字节表示2个像素
            int y0 = data_[i];
            int u = data_[i + 1];
            int y1 = data_[i + 2];
            int v = data_[i + 3];

            int c = y0 - 16;
            int d = u - 128;
            int e = v - 128;

            int r0 = clamp((298 * c + 409 * e + 128) >> 8, 0, 255);
            int g0 = clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255);
            int b0 = clamp((298 * c + 516 * d + 128) >> 8, 0, 255);

            c = y1 - 16;
            int r1 = clamp((298 * c + 409 * e + 128) >> 8, 0, 255);
            int g1 = clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255);
            int b1 = clamp((298 * c + 516 * d + 128) >> 8, 0, 255);

            // 写入两个像素数据
            *line++ = r0; *line++ = g0; *line++ = b0; // 第一个像素
            *line++ = r1; *line++ = g1; *line++ = b1; // 第二个像素
        }
    }

    image_ = image;  // 返回转换后的 QImage
}

// void Vvideo::MJPG2RGB(QImage &image_, void *data, size_t length){
//     QByteArray mjpegData(static_cast<const char*>(data), length);
//     QImage image;
//     QBuffer buffer(&mjpegData);
//     buffer.open(QIODevice::ReadOnly);
//     QImageReader reader(&buffer, "JPEG");  // 指定为 JPEG 格式
//     if (reader.read(&image)) {
//         image_ = image;  // 返回解码后的图像
//     } else {
//         qWarning() << "Failed to decode MJPEG frame!";
//         return;  // 返回空图像
//     }
// }

void Vvideo::MJPG2RGB(QImage &image_, void *data, size_t length) {
    tjhandle handle = tjInitDecompress();
    if (!handle) {
        qWarning() << "Failed to initialize TurboJPEG decompressor";
        return;
    }

    int width, height, subsamp, colorspace;
    if (tjDecompressHeader3(handle, static_cast<unsigned char*>(data), length, 
                            &width, &height, &subsamp, &colorspace) != 0) {
        qWarning() << "Failed to read MJPEG header:" << tjGetErrorStr();
        tjDestroy(handle);
        return;
    }

    QImage image(width, height, QImage::Format_RGB888);
    if (tjDecompress2(handle, static_cast<unsigned char*>(data), length,
                      image.bits(), width, 0, height, TJPF_RGB, TJFLAG_FASTDCT) != 0) {
        qWarning() << "Failed to decompress MJPEG frame:" << tjGetErrorStr();
        tjDestroy(handle);
        return;
    }

    tjDestroy(handle);
    image_ = image;  // 返回解码后的图像
}

void Vvideo::NV12ToRGB(QImage &image_, void *data_y, size_t len_y, void *data_uv, size_t len_uv) {
    QImage image(w, h, QImage::Format_RGB888);
    unsigned char* y_data = static_cast<unsigned char*>(data_y);
    unsigned char* uv_data = static_cast<unsigned char*>(data_uv);

    for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
            int y = y_data[i * w + j];
            int u = uv_data[(i / 2) * w + (j / 2) * 2] - 128;
            int v = uv_data[(i / 2) * w + (j / 2) * 2 + 1] - 128;

            int r = y + 1.403 * v;
            int g = y - 0.344 * u - 0.714 * v;
            int b = y + 1.770 * u;

            r = clamp(r, 0, 255);
            g = clamp(g, 0, 255);
            b = clamp(b, 0, 255);

            image.setPixel(j, i, qRgb(r, g, b));
        }
    }
    image_ = image;
}

void Vvideo::updateFrame() {
    QMutexLocker locker(&mutex);  // 锁定队列
    if (!frameQueue.isEmpty()) {
        QImage frame = frameQueue.dequeue();  // 从队列中取出一帧
        emit frameAvailable(frame);  // 发射信号，通知主界面更新显示
    }
}

int Vvideo::closeDevice() {
    frameQueue.clear();

    // 停止采集并释放映射
    if (ioctl(fd, VIDIOC_STREAMOFF, &buffer.type) == -1) {
        perror("Failed to stop streaming");
        close(fd);
        return -1;
    }

    if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
        // 多平面缓冲区的解映射
        for (int i = 0; i < BUFCOUNT; i++) {
            for (int plane = 0; plane < framebuf[i].plane_count; plane++) {
                if (framebuf[i].start[plane] && framebuf[i].start[plane] != MAP_FAILED) {
                    munmap(framebuf[i].start[plane], framebuf[i].length[plane]);
                    framebuf[i].start[plane] = nullptr; // 释放映射后，避免再次操作
                }
            }
        }
    } else {
        // 单平面缓冲区的解映射
        for (int i = 0; i < BUFCOUNT; i++) {  // 修正了 <= BUFCOUNT 的问题
            if (framebuf[i].start[0]) {
                munmap(framebuf[i].start[0], framebuf[i].length[0]);
                framebuf[i].start[0] = nullptr; // 防止重复操作
            }
        }
    }

    // 关闭设备
    close(fd);
    fd = -1;
    qDebug() << "--------------";

    return 0;
}
