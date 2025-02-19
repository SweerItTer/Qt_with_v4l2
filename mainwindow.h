#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QTimer>

#include "v4l2_video.h"

#include <memory>
#include <pthread.h>
using namespace std;

class MainWindow : public QObject
{
	Q_OBJECT
    // 这里需要和qml联动,获取到下拉框的选择
    Q_PROPERTY(int devices READ devices WRITE setDevices NOTIFY devicesChanged)
    Q_PROPERTY(int pixFormat READ pixFormat WRITE setPixFormat NOTIFY pixFormatChanged)
    Q_PROPERTY(int resolutions READ resolutions WRITE setResolutions NOTIFY resolutionsChanged)

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
    
    // QComboBox *devicesComboBox = nullptr;
    // QComboBox *pixFormatComboBox = nullptr;
    // QComboBox *resolutionsComboBox = nullptr;
    // QLabel *displayLabel = nullptr;
    std::unique_ptr<Vvideo> m_captureThread;    // Vvideo 对象指针
    std::thread threadHandle;                   // 标准库线程对象

    QImage frame_;
    QTimer *timer = nullptr;

	int fd = -1;

    const int REFERENCE_WIDTH = 1920;
    const int REFERENCE_HEIGHT = 1080;

};
#endif // MAINWINDOW_H
