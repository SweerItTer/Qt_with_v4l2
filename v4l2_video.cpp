#include "v4l2_video.h"
#include <QImageReader>
#include <QBuffer>
#include <QElapsedTimer>

#include <sys/mman.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

#include <turbojpeg.h>

#define BUFCOUNT 18
#define FMT_NUM_PLANES 2

inline int clamp(int value, int min, int max)
{
    return std::max(min, std::min(value, max));
}

v4l2_buf_type type;
Vvideo::Vvideo(const bool& is_M_, MyWidget *Label, QObject *parent)
    : fd(-1), is_M(is_M_), displayLabel(Label)
{
    type = is_M ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    framebuf = new video_buf_t[BUFCOUNT];

}

Vvideo::~Vvideo(){
    quit_ = 1;
    if (captureThread_.joinable()) captureThread_.join();
    if (processThread_.joinable()) processThread_.join();
    closeDevice();

    // 释放缓冲区
    if (framebuf != nullptr) {
        delete[] framebuf;
        framebuf = nullptr;
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
    for (int num = 0; num < BUFCOUNT; num++) {
        framebuf[num].fm[0].in_use = false;  // 初始状态未使用
    }
    
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

        framebuf[num].fm[0].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
        if (framebuf[num].fm[0].start == MAP_FAILED) {
            perror("Failed to map buffer");
            goto cleanup;
        }
        framebuf[num].fm[0].length = buffer.length;

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
        if (framebuf[i].fm[0].start && framebuf[i].fm[0].start != MAP_FAILED) {
            munmap(framebuf[i].fm[0].start, framebuf[i].fm[0].length);
            framebuf[i].fm[0].start = nullptr; // 清理映射
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
            framebuf[num].fm[plane].length = buffer.m.planes[plane].length;
            framebuf[num].fm[plane].start = mmap(
                NULL, framebuf[num].fm[plane].length, PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, buffer.m.planes[plane].m.mem_offset);

            if (framebuf[num].fm[plane].start == MAP_FAILED) {
                perror("Failed to map plane buffer");
                for (int j = 0; j < plane; j++) {
                    if (framebuf[num].fm[j].start != MAP_FAILED) {
                        munmap(framebuf[num].fm[j].start, framebuf[num].fm[j].length);
                        framebuf[num].fm[j].start = nullptr;
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
            if (framebuf[i].fm[plane].start && framebuf[i].fm[plane].start != MAP_FAILED) {
                munmap(framebuf[i].fm[plane].start, framebuf[i].fm[plane].length);
                framebuf[i].fm[0].start = nullptr; // 清理映射
            }
        }
    }
    close(fd);
    return -1;
}



int Vvideo::captureFrame() {
    while(!quit_)
    {
        // 限制缓存队列长度
        if (frameIndexQueue.size() > 14) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 等待队列有数据
            continue;
        }

        // 初始化结构体
        struct v4l2_plane planes[FMT_NUM_PLANES];
        memset(planes, 0, sizeof(planes));
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = type;
        buffer.memory = V4L2_MEMORY_MMAP;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == type) {
            buffer.m.planes = planes;
            buffer.length = FMT_NUM_PLANES;
        }

        // 监视文件超时
        struct timeval tv;
        tv.tv_sec = 1; // 1秒
        tv.tv_usec = 0;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        int r = select(fd + 1, &fds, NULL, NULL, &tv);
        if (r == -1) {
            perror("select");
            return -1;
        } else if (r == 0) {
            qDebug() << "Timeout waiting for buffer";
            continue;
        }
        // 出列
        if (ioctl(fd, VIDIOC_DQBUF, &buffer) == -1) {
            perror("Failed to dequeue buffer");
            return -1;
        }

        int buf_index = buffer.index;

        // 如果该缓冲区正在被 `processFrame()` 处理，则重新入队
        if (framebuf[buf_index].fm[0].in_use == true) {
            if (ioctl(fd, VIDIOC_QBUF, &buffer) == -1) {
                perror("Failed to queue buffer");
            }
            continue;
        }
        
        
        // 标记缓冲区正在使用
        framebuf[buf_index].fm[0].in_use = true;

        // 入队处理
        frameIndexQueue.enqueue(buf_index);
    }
    return 0;
}

void Vvideo::processFrame(MyWidget *displayLabel) {
    int buf_index;
    uint wait = 0;
    QElapsedTimer frameTimer;
    frameTimer.start();
    const int targetFrameTime = 33; // 约30fps

    while (!quit_) {
        // 帧率控制
        qint64 elapsed = frameTimer.elapsed();
        if (elapsed < targetFrameTime) {
            std::this_thread::sleep_for(std::chrono::milliseconds(targetFrameTime - elapsed));
            continue;
        }
        frameTimer.restart();

        // 限制队列大小为15帧,避免内存占用过高
        while(frameQueue.size() > 15) {
            TimedImage oldFrame;
            frameQueue.try_dequeue(oldFrame); // 丢弃旧帧
        }
        // 从队列中取出帧
        if (!frameIndexQueue.try_dequeue(buf_index)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 等待队列有数据
            continue;
        }
        // 若数据长度为0,忽略
        if (framebuf[buf_index].fm[0].length == 0) continue;

        QImage image_(w, h, QImage::Format_RGB888);
        bool conversionSuccess = false;

        // 图像格式转换
        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == type) {
            switch(fmt) {
                case V4L2_PIX_FMT_NV12:
                    conversionSuccess = NV12ToRGB(image_, 
                        framebuf[buf_index].fm[0].start, 
                        framebuf[buf_index].fm[0].length,
                        framebuf[buf_index].fm[1].start, 
                        framebuf[buf_index].fm[1].length);
                    break;
                case V4L2_PIX_FMT_MJPEG:
                case V4L2_PIX_FMT_JPEG:
                    conversionSuccess = MJPG2RGB(image_, 
                        framebuf[buf_index].fm[0].start, 
                        framebuf[buf_index].fm[0].length);
                    break;
                case V4L2_PIX_FMT_YUYV:
                    conversionSuccess = YUYV2RGB(image_, 
                        framebuf[buf_index].fm[0].start, 
                        framebuf[buf_index].fm[0].length);
                    break;
                default:
                    qDebug() << "Unsupported format";
                    image_ = QImage();
                    break;
            }
        } else {
            conversionSuccess = MJPG2RGB(image_, framebuf[buf_index].fm[0].start, 
                framebuf[buf_index].fm[0].length);
        }

        // 重新入队缓冲区
        struct v4l2_buffer qbuf;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        memset(&planes, 0, sizeof(planes));
        memset(&qbuf, 0, sizeof(qbuf));
        qbuf.type = type;
        qbuf.index = buf_index;
        qbuf.memory = V4L2_MEMORY_MMAP;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == type) {
            qbuf.m.planes = planes;
            qbuf.length = FMT_NUM_PLANES;
        }
        framebuf[buf_index].fm[0].in_use = false;
        // 缓冲区重新入队
        if (ioctl(fd, VIDIOC_QBUF, &qbuf) == -1) {
            perror("Failed to queue buffer");
        }

        if (!conversionSuccess || image_.isNull()) continue;
        #ifdef RV1126
            // 旋转图像以适应竖屏显示
            image_ = image_.transformed(QMatrix().rotate(270));
        #endif 
        // 缩放图像并入队
        TimedImage timedImage;
        
        // 使用Qt::FastTransformation代替SmoothTransformation以提高性能
        timedImage.image = image_.scaled(displayLabel->size(), 
            Qt::KeepAspectRatio, Qt::FastTransformation);
        timedImage.timestamp = QTime::currentTime();
        frameQueue.enqueue(timedImage);
    }
}

bool Vvideo::MJPG2RGB(QImage &image_, void *data, size_t length) {
    static tjhandle handle = tjInitDecompress(); // 静态handle避免重复初始化
    if (!handle) {
        qWarning() << "Failed to initialize TurboJPEG decompressor";
        return false;
    }

    int width, height, subsamp, colorspace;
    if (tjDecompressHeader3(handle, static_cast<unsigned char*>(data), length, 
                            &width, &height, &subsamp, &colorspace) != 0) {
        qWarning() << "Failed to read MJPEG header:" << tjGetErrorStr();
        return false;
    }
    // 直接操作image_减少额外开销
    if (tjDecompress2(handle, static_cast<unsigned char*>(data), length,
                      image_.bits(), width, 0, height, TJPF_RGB, TJFLAG_FASTDCT) != 0) {
        qWarning() << "Failed to decompress MJPEG frame:" << tjGetErrorStr();
        return false;
    }

    return true;
}

/* 尝试直接操作image_(引用)减少额外开销 */
bool Vvideo::YUYV2RGB(QImage &image_, void *data, size_t length) {
    // 确保数据有效
    if (!data || length == 0) {
        qWarning() << "Invalid data for YUYV to RGB conversion.";
        return false;
    }

    // 1. 使用 ARGB 作为中间格式
    std::vector<uint8_t> argb_buffer(w * h * 4); // ARGB 缓冲区
    int ret = libyuv::YUY2ToARGB(
        static_cast<const uint8_t*>(data),  // 输入 YUYV 数据
        w * 2,                              // YUYV 的步长（每行字节数）
        argb_buffer.data(),                 // 输出 ARGB 数据
        w * 4,                              // ARGB 的步长（每行字节数）
        w, h                                // 宽高
    );
    if (ret < 0){ 
        qWarning() << "Failed to convert YUYV to ARGB.";
        return false; 
    }
    // 2. 手动交换 ARGB 中的 R 和 B 通道
    for (size_t i = 0; i < w * h; ++i) {
        uint8_t *pixel = &argb_buffer[i * 4];
        std::swap(pixel[0], pixel[2]);  // 交换 R 和 B
    }

    // 3. 将 ARGB 转换为 RGB24（丢弃 Alpha 通道）
    ret = libyuv::ARGBToRGB24(
        argb_buffer.data(),    // 输入 ARGB 数据
        w * 4,                 // ARGB 的步长
        image_.bits(),         // 输出 RGB24 数据
        w * 3,                 // RGB24 的步长
        w, h
    );
    if (ret < 0){ 
        qWarning() << "Failed to convert ARGB to RGB24.";
        return false; 
    }

    return (ret == 0);
}

bool Vvideo::NV12ToRGB(QImage &image_, void *data_y, size_t len_y, void *data_uv, size_t len_uv) {
    // 确保数据有效
    if (!data_y || !data_uv || len_y == 0 || len_uv == 0) {
        qWarning() << "Invalid data for NV12 to RGB conversion.";
        return false;
    }

    // 使用libyuv转换NV12到RGB
    int ret = libyuv::NV12ToRGB24(
        static_cast<const uint8_t*>(data_y),    // Y平面
        w,                                      // Y步长
        static_cast<const uint8_t*>(data_uv),   // UV平面
        w,                                      // UV步长（NV12中UV交错）
        image_.bits(),                           // 输出RGB数据
        w * 3,                                  // RGB步长
        w, h                                    // 宽高
    );
    if(ret < 0){
        qWarning() << "Failed to convert NV12 to RGB.";
    }

    return (ret == 0);
}

void Vvideo::updateImage()
{
    
    TimedImage timedImage;
    if (!frameQueue.try_dequeue(timedImage)) return; // 从队列中取出带时间戳的图像
    // 如果正在更新中,直接退出
    if (displayLabel->status != Idle) return;
    if (timedImage.image.isNull()) return;

    displayLabel->updateImage(timedImage.timestamp, timedImage.image);

}

void Vvideo::takePic(QImage &img)
{
    TimedImage timedImage;
    img = frameQueue.dequeue().image;
}

int Vvideo::closeDevice()
{
    frameIndexQueue.clear(); // 清空队列
    frameQueue.clear();
    // QImageframes.clear();
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
                if (framebuf[i].fm[plane].start && framebuf[i].fm[plane].start != MAP_FAILED) {
                    munmap(framebuf[i].fm[plane].start, framebuf[i].fm[plane].length);
                    framebuf[i].fm[plane].start = nullptr; // 释放映射后，避免再次操作
                }
            }
        }
    } else {
        // 单平面缓冲区的解映射
        for (int i = 0; i < BUFCOUNT; i++) {  // 修正了 <= BUFCOUNT 的问题
            if (framebuf[i].fm[0].start) {
                munmap(framebuf[i].fm[0].start, framebuf[i].fm[0].length);
                framebuf[i].fm[0].start = nullptr; // 防止重复操作
            }
        }
    }

    // 关闭设备
    close(fd);
    fd = -1;
    qDebug() << "--------------";

    return 0;
}
