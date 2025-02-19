
#ifndef MYWIDGET_H
#define MYWIDGET_H

#include <QImage>
#include <QWidget>
#include <QPainter>

class MyWidget : public QWidget {
public:
    MyWidget(QWidget *parent = nullptr) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(this);
        // 绘制 QImage 或 QPixmap
        painter.drawImage(0, 0, image_);
    }

public:
    void updateImage(const QImage &image) {
        image_ = image;
        update();  // 手动触发重绘
    }

private:
    QImage image_;
};

#endif // MYWIDGET_H