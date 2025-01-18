#include "v4l2_video.h"
#include <QDebug>

#include <unistd.h>
#include <fcntl.h>
#include <linux/videodev2.h>

Vvideo::Vvideo(const QString &device, const __u32 &format, const QString &_resolution,const bool& is_M_, QObject *parent)
    : QObject(parent), deviceName(device), pixformat(format), resolution(_resolution), fd(-1), is_M(is_M_)
{}

Vvideo::~Vvideo(){
    closeDevice();
}

bool Vvideo::openDevice()
{
    // 1.打开设备
    fd = open(deviceName.toLocal8Bit().constData(), O_RDWR);
    if (fd < 0) {
        // 打开设备失败
        qDebug() << "Failed to open device" << deviceName;
        return false;
    }
    qDebug() << "open device:" << deviceName << "\npixformat:" << pixformat << "\nresolution:" << resolution;
    // 2.配置设备
    // struct v4l2_format v_fmt;
    // int ret = ioctl(fd, VIDIOC_S_FMT, &v_fmt);
    // 3.申请内核缓冲区队列

    // 4.映射缓冲区到用户空间
    return true;
}

int Vvideo::startCapture()
{
    // 5.开始采集
    return 0;
}

QImage Vvideo::captureFrame()
{   
    // 6.数据采集及处理(线程锁)
    return QImage();
}
int Vvideo::stopCapture()
{
    // 7.停止采集(释放映射)
    return 0;
}


bool Vvideo::closeDevice()
{   
    stopCapture();
    // 8.关闭设备
    if (fd != -1) {
        close(fd);
        fd = -1;
        qDebug() << "--------------";

        return true;
    }
    return false;
}
