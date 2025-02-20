
#ifndef MYWIDGET_H
#define MYWIDGET_H

#include <QImage>
#include <QDebug>
#include <QWidget>
#include <QPainter>
#include <QTime>

enum UpdateStatus {
    Idle,        // 空闲状态，允许更新
    Updating     // 正在更新中，禁止获取新帧
};

class MyWidget : public QWidget {
public:
    MyWidget(QWidget *parent = nullptr) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent *event) override {
        status = Updating;
        QPainter painter(this);
        // 绘制 QImage 或 QPixmap
        painter.drawImage(0, 0, image_);
        // 计算帧的加载时间
        qint64 loadTime = time_.elapsed();
        qDebug() << "Frame load time (ms):" << loadTime;
        qDebug() << "FPS:" << 1000.0 / loadTime; // 每帧加载时间对应的帧率
        status = Idle;
    }

public:
    void updateImage(const QTime &time, const QImage &image) {
        time_ = time;
        image_ = image;
        update();  // 手动触发重绘
    }

    UpdateStatus status = Idle;
private:
    QImage image_;
    QTime time_;
};

#endif // MYWIDGET_H