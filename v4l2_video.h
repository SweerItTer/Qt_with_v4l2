#ifndef V4L2_VIDEO_H
#define V4L2_VIDEO_H

#include <stdio.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <QObject>
#include <QImage>

using namespace std;

class Vvideo : public QObject
{
    Q_OBJECT
public:
    public:
    explicit Vvideo(const QString &device, const __u32 &format, const QString &_resolution, const bool& is_M_,
     QObject *parent = nullptr);
    ~Vvideo();

    bool openDevice();
    
    int startCapture();
    int stopCapture();
    QImage captureFrame();

    bool closeDevice();

signals:
    void frameAvailable(const QImage &frame);

private:
    int fd;
    bool is_M;
    QString deviceName;
    __u32 pixformat;
    QString resolution;
    // 其他必要的成员变量
};


#endif // V4L2_VIDEO_H