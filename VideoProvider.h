#include <QQuickImageProvider>
#include <QImage>
#include "v4l2_video.h"

class VideoProvider : public QQuickImageProvider {
public:
    VideoProvider(Vvideo* camera) 
        : QQuickImageProvider(QQuickImageProvider::Image),
          m_camera(camera) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
        QImage frame;
        m_camera->getLatestFrame(frame); // 从Vvideo获取帧
        return frame;
    }

private:
    Vvideo* m_camera;
};