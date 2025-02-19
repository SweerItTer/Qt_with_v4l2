#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QComboBox>
#include <QTimer>

#include "v4l2_video.h"
#include "mywidget.h"

#include <memory>
#include <pthread.h>
using namespace std;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QWidget
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private slots:
    void on_takepic_pressed();
    void on_takepic_released();
    void on_open_pb_released();
    void on_showimg_released();

    void on_devices_currentIndexChanged(int index);
    void fillComboBoxWithResolutions(int a);

private:
    QString fourccToString(__u32 fourcc) {
        return QString("%1%2%3%4")
            .arg(static_cast<char>(fourcc & 0xFF))
            .arg(static_cast<char>((fourcc >> 8) & 0xFF))
            .arg(static_cast<char>((fourcc >> 16) & 0xFF))
            .arg(static_cast<char>((fourcc >> 24) & 0xFF));
    }

    // 填充下拉框
    void fillComboBoxWithV4L2Devices();
    void fillComboBoxWithPixFormats(bool isMultiPlane);
    void fillComboBoxWithResolutions(bool isMultiPlane);

    QString findOldestImage(const QString &folderPath);
    void killThread();
    void setIcon(QString &fileName);
    
    Ui::MainWindow *ui;
    QComboBox *devicesComboBox = nullptr;
    QComboBox *pixFormatComboBox = nullptr;
    QComboBox *resolutionsComboBox = nullptr;
    MyWidget *displayWidget = nullptr;
    std::unique_ptr<Vvideo> m_captureThread;    // Vvideo 对象指针
    std::thread threadHandle;                   // 标准库线程对象

    QImage frame_;
    QTimer *timer = nullptr;

	int fd = -1;

    const int REFERENCE_WIDTH = 1920;
    const int REFERENCE_HEIGHT = 1080;

};
#endif // MAINWINDOW_H
