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
	, m_captureThread(nullptr)
{
	ui->setupUi(this);
	ui->takepic->setIconSize(QSize(40, 40)); // 设置图标大小
	ui->takepic->setIcon(QIcon(":/icon/icon/takepic_1.svg")); // 设置SVG图标

    ui->Display->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ui->Display->setAlignment(Qt::AlignCenter);  // 图像居中显示

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
    if(m_captureThread){
        m_captureThread->quit_ = 1;
        m_captureThread->quit();
        m_captureThread->wait();
        delete m_captureThread;
        m_captureThread = nullptr;
    }
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
bool isV4L2VideoCaptureDevice(const QString &path, bool& isMultiPlane) {
    int fd = open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK); // 使用非阻塞模式打开设备
    if (fd < 0) {
        return false;
    }

    v4l2_capability cap;
    // 查询设备功能
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        close(fd);
        return false;
    }

    // 检查设备是否支持视频捕获
    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE) {
        isMultiPlane = false;
        close(fd);
        return true;
    }
    // 检查设备是否支持多平面视频捕获
    if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) == V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
        isMultiPlane = true;
        close(fd);
        return true;
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
    foreach (const QString &device, devices) {
        QString path = devDir.path() + "/" + device;
        if (isCharacterDevice(path)) {
            bool isMultiPlane = false;
            if (isV4L2VideoCaptureDevice(path, isMultiPlane)) {
                devicesComboBox->addItem(path, path);
                v4l2Devices.append({path, isMultiPlane});
                deviceFound = true;
            }
        }
    }

    if (!deviceFound) {
        QMessageBox::warning(this, tr("Device Not Found"), tr("No video capture devices found."));
        return;
    }
    on_devices_currentIndexChanged(0); 
}
// 槽函数:更新格式和分辨率
void MainWindow::on_devices_currentIndexChanged(int index) {
    disconnect(pixFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(fillComboBoxWithResolutions(int)));

    if(m_captureThread){
        m_captureThread->quit_ = 1;
        m_captureThread->quit();
        m_captureThread->wait();
        delete m_captureThread;
        m_captureThread = nullptr;
    }
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
        global_M = it->isMultiPlane;

        fillComboBoxWithPixFormats(it->isMultiPlane);
        fillComboBoxWithResolutions(it->isMultiPlane);

        ::close(fd);
    }
    connect(pixFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(fillComboBoxWithResolutions(int)));

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
    if(resolutionsComboBox->count() != 0) return;
    if(m_captureThread){
        m_captureThread->quit_ = 1;
        m_captureThread->quit();
        m_captureThread->wait();
        delete m_captureThread;
        m_captureThread = nullptr;
    }
    fd = open(devicesComboBox->currentText().toLocal8Bit().constData(), O_RDWR);
    if (fd < 0) {
        return;
    }
    resolutionsComboBox->clear();
    fillComboBoxWithResolutions(false);
    ::close(fd);
}

void MainWindow::on_open_pb_released()
{
    if(m_captureThread){
        m_captureThread->quit_ = 1;
        m_captureThread->quit();
        m_captureThread->wait();
        delete m_captureThread;
        m_captureThread = nullptr;
    }
    m_captureThread = new Vvideo(global_M);  

    // 初始化V4L2设备
    if (m_captureThread->openDevice(devicesComboBox->currentText()) < 0) {
        QMessageBox::critical(this, "error", "Open device failed.");
        return;
    }
    
    // 设置视频格式
    QString resolution = resolutionsComboBox->currentText();
    
    int xIndex = resolution.indexOf('x');
    __u32 width = resolution.left(xIndex).toUInt();
    __u32 height = resolution.mid(xIndex + 1).toUInt();
    if (m_captureThread->setFormat(width, height, pixFormatComboBox->currentData().toUInt()) < 0) {
        QMessageBox::critical(this, "error", "set pixformat failed.");
        return;
    }
    
    // 初始化缓冲区
    if (m_captureThread->initBuffers() < 0) {
        QMessageBox::critical(this, "error", "initial map failed.");
        return;
    }
    
    // 开始视频流
    m_captureThread->start();    
    connect(m_captureThread, &Vvideo::frameAvailable, this, &MainWindow::updateFrame, Qt::UniqueConnection);

}

void MainWindow::updateFrame(const QImage& frame)
{
    QImage frame_ = frame;
    if(frame_.isNull()) return;
    ui->Display->setPixmap(QPixmap::fromImage(frame_).scaled(
        ui->Display->width(), ui->Display->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}


void MainWindow::on_takepic_pressed()
{
	ui->takepic->setIcon(QIcon(":/icon/icon/takepic_2.svg")); // 设置SVG图标
}

void MainWindow::on_takepic_released()
{
	ui->takepic->setIcon(QIcon(":/icon/icon/takepic_1.svg")); // 设置SVG图标
}