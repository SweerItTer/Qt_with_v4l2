#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QComboBox>

#include "v4l2_video.h"

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

    void on_devices_currentIndexChanged(int index);
    void fillComboBoxWithResolutions(int a);

private:
    // 填充下拉框
    void fillComboBoxWithV4L2Devices();
    void fillComboBoxWithPixFormats(bool isMultiPlane);
    void fillComboBoxWithResolutions(bool isMultiPlane);


    Ui::MainWindow *ui;
    QComboBox *devicesComboBox = nullptr;
    QComboBox *pixFormatComboBox = nullptr;
    QComboBox *resolutionsComboBox = nullptr;
    Vvideo *Vv = nullptr;

	int fd = -1;
};
#endif // MAINWINDOW_H
