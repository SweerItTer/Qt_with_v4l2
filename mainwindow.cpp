#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDir>
#include <QString>
#include <QDebug>
#include <QMessageBox>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/videodev2.h>

// 设备信息结构
struct DeviceInfo {
    QString path;
    bool isMultiPlane;
};
bool global_M = false;
// 全局或类成员变量，用于存储设备信息
QVector<DeviceInfo> v4l2Devices;

MainWindow::MainWindow(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	ui->takepic->setIconSize(QSize(40, 40)); // 设置图标大小
	ui->takepic->setIcon(QIcon(":/icon/icon/takepic_1.svg")); // 设置SVG图标

	devicesComboBox = ui->devices;
	pixFormatComboBox = ui->pixformat;
	resolutionsComboBox = ui->resolutions;

	fillComboBoxWithV4L2Devices();

	connect(devicesComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_devices_currentIndexChanged(int)));
    connect(pixFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(fillComboBoxWithResolutions(int)));

}

MainWindow::~MainWindow()
{
	if(fd != -1){
		::close(fd);
	}
    if(Vv) delete Vv;
    delete devicesComboBox;
    delete pixFormatComboBox;
    delete resolutionsComboBox;
	delete ui;
}

// 检查给定的路径是否是一个字符设备
bool isCharacterDevice(const QString &path) {
    struct stat st;
    if (stat(path.toLocal8Bit().constData(), &st) == 0) {
        return S_ISCHR(st.st_mode);
    }
    return false;
}

// 检查给定的设备路径是否是V4L2视频设备
bool isV4L2Device(const QString &path, bool& isMultiPlane) {
    int fd = open(path.toLocal8Bit().constData(), O_RDWR);
    if (fd < 0) {
        return false;
    }

    v4l2_capability cap;
    // 以适应V4L2_CAP_VIDEO_CAPTURE和V4L2_CAP_VIDEO_CAPTURE_MPLANE
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE) {
            isMultiPlane = false;
            close(fd);
            return true;
        }
        if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) == V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
            isMultiPlane = true;
            close(fd);
            return true;
        }
    }
    close(fd);
    return false;   
}

void MainWindow::fillComboBoxWithV4L2Devices() {
    QDir devDir("/dev");
    QStringList filters;
    filters << "video*"; // 过滤出以 "video" 开头的设备文件
    QStringList devices = devDir.entryList(filters, QDir::Files | QDir::System);

	bool deviceFound = false;
    bool isMultiPlane = false;
    foreach (const QString &device, devices) {
        QString path = devDir.path() + "/" + device;
        if (isCharacterDevice(path) && isV4L2Device(path, isMultiPlane)) {
            devicesComboBox->addItem(path, path);
            v4l2Devices.append({path, isMultiPlane});
            deviceFound = true;
        }
    }

    if (!deviceFound) {
        QMessageBox::warning(this, tr("Device Not Found"), tr("No devices found."));
        return;
    }
    on_devices_currentIndexChanged(0);
}
// 槽函数:更新格式和分辨率
void MainWindow::on_devices_currentIndexChanged(int index) {
    QString devicePath = devicesComboBox->itemData(index).toString();

    auto it = std::find_if(v4l2Devices.begin(), v4l2Devices.end(),
        [&](const DeviceInfo &info) { return info.path == devicePath; }); // 判断是否存在设备

    if (it != v4l2Devices.end()) {
        fd = open(it->path.toLocal8Bit().constData(), O_RDWR);
        if (fd < 0) {
            return;
        }

        pixFormatComboBox->clear();
        resolutionsComboBox->clear();
        fillComboBoxWithPixFormats(it->isMultiPlane);
        fillComboBoxWithResolutions(it->isMultiPlane);
        global_M = it->isMultiPlane;
        ::close(fd);
    }
}

void MainWindow::fillComboBoxWithPixFormats(bool isMultiPlane) {
    struct v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = isMultiPlane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
        pixFormatComboBox->addItem(QString::fromUtf8(reinterpret_cast<const char*>(fmt.description)), fmt.pixelformat);
        fmt.index++;
    }
}

void MainWindow::fillComboBoxWithResolutions(bool isMultiPlane) {
    struct v4l2_frmsizeenum frmsize;
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.pixel_format = pixFormatComboBox->currentData().toUInt();

    while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            QString resolution = QString::number(frmsize.discrete.width) + "x" + QString::number(frmsize.discrete.height);
            resolutionsComboBox->addItem(resolution);
        }
        frmsize.index++;
    }
}

void MainWindow::fillComboBoxWithResolutions(int a) {
    fd = open(devicesComboBox->currentText().toLocal8Bit().constData(), O_RDWR);
    if (fd < 0) {
        return;
    }
    resolutionsComboBox->clear();
    fillComboBoxWithResolutions(false);
    ::close(fd);
}
void MainWindow::on_takepic_pressed()
{
	ui->takepic->setIcon(QIcon(":/icon/icon/takepic_2.svg")); // 设置SVG图标
}

void MainWindow::on_takepic_released()
{
	ui->takepic->setIcon(QIcon(":/icon/icon/takepic_1.svg")); // 设置SVG图标
}

void MainWindow::on_open_pb_released()
{
    if(Vv){
        while(! Vv->closeDevice() ); // 成功关闭设备
        delete Vv;
    }
    Vv = new Vvideo(devicesComboBox->currentText(), pixFormatComboBox->currentData().toUInt(), resolutionsComboBox->currentText(), global_M);
    bool ret = Vv->openDevice();
    if(ret == false){ // 打开失败
        QMessageBox::critical(this, tr("Device Error"), tr("Failed to open the selected device.\nPlease check if the device is connected properly or in use by another application."));
    }
}
