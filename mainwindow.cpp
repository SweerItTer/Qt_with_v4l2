#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDir>
#include <QString>
#include <QDebug>
#include <QMessageBox>
#include <QDateTime>
#include <QImageWriter>

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

    QString fileName = findOldestImage(QCoreApplication::applicationDirPath() + "/photos/");
    ui->showimg->setStyleSheet("border-image:url("+fileName+")");

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
// 填充设备列表
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
// 根据选择设备修改格式和分辨率
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
// 填充像素格式列表
void MainWindow::fillComboBoxWithPixFormats(bool isMultiPlane) {
    struct v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = isMultiPlane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // 检查设备是否支持多平面缓冲区类型或单平面缓冲区类型
    if (isMultiPlane) qDebug() << "Device support multi-plane video capture.";
    else qDebug() << "Device support single-plane video capture.";
    

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0) {
        QString pixFmtStr = fourccToString(fmt.pixelformat);
        pixFormatComboBox->addItem(pixFmtStr, fmt.pixelformat); // 将格式代码作为值存储
        qDebug() << "Found pixel format:" << pixFmtStr;
        fmt.index++;
    }

    if (errno != EINVAL) {  // 如果错误不是因为格式索引超出范围
        qDebug() << "Error enumerating formats:" << strerror(errno);
        // 这里可以添加更多的错误处理逻辑，比如弹出错误提示或者记录日志
    }
}
// 填充分辨率列表
void MainWindow::fillComboBoxWithResolutions(bool isMultiPlane) {
    struct v4l2_frmsizeenum frmsize;
    memset(&frmsize, 0, sizeof(frmsize));
    frmsize.pixel_format = pixFormatComboBox->currentData().toUInt();
    frmsize.index = 0;

    while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {        // 离散的分辨率
            QString resolution = QString::number(frmsize.discrete.width) + "x" + QString::number(frmsize.discrete.height);
            resolutionsComboBox->addItem(resolution);
            // qDebug() << "Found resolution:" << resolution;
        } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {// 步进类型的分辨率 IMX415
            int minWidth = frmsize.stepwise.min_width;
            int minHeight = frmsize.stepwise.min_height;
            int maxWidth = frmsize.stepwise.max_width;
            int maxHeight = frmsize.stepwise.max_height;

            // 设置要展示的分辨率数量
            int totalResolutions = 10;

            // 计算宽度和高度的增量
            float widthStep = (float)(maxWidth - minWidth) / (totalResolutions - 1);
            float heightStep = (float)(maxHeight - minHeight) / (totalResolutions - 1);

            // 遍历生成 10 个等比例分辨率
            for (int i = 0; i < totalResolutions; ++i) {
                // 计算当前分辨率
                int width = static_cast<int>(minWidth + i * widthStep);
                int height = static_cast<int>(minHeight + i * heightStep);

                // 添加到下拉框中
                QString resolution = QString::number(width) + "x" + QString::number(height);
                resolutionsComboBox->addItem(resolution);
                // qDebug() << "Found resolution:" << resolution;
            }
        }
        frmsize.index++;
    }

    if (errno != EINVAL) {  // 如果错误不是因为格式索引超出范围
        qDebug() << "Error enumerating resolutions:" << strerror(errno);
        // 这里可以添加更多的错误处理逻辑，比如弹出错误提示或者记录日志
        resolutionsComboBox->addItem("1280x720");
        resolutionsComboBox->addItem("1920x1080");
    }
}
// 重载槽函数
void MainWindow::fillComboBoxWithResolutions(int a) {
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
// 打开摄像头
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
// 更新图像
void MainWindow::updateFrame(const QImage& frame)
{
    frame_ = frame;
    if(frame_.isNull()) return;
    ui->Display->setPixmap(QPixmap::fromImage(frame_).scaled(
        ui->Display->width(), ui->Display->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
// 美化UI用(按钮图标更新)
void MainWindow::on_takepic_pressed()
{
	ui->takepic->setIcon(QIcon(":/icon/icon/takepic_2.svg")); // 设置SVG图标
}
// 保存图像
void MainWindow::on_takepic_released()
{
	ui->takepic->setIcon(QIcon(":/icon/icon/takepic_1.svg")); // 设置SVG图标
    
    QImage img = frame_;
    // 获取当前时间
    QDateTime currentDateTime = QDateTime::currentDateTime();
    // 将当前时间格式化为字符串（年月日时分秒）
    QString dateTimeString = currentDateTime.toString("yyyyMMddhhmmss");
    QString path = QCoreApplication::applicationDirPath() + "/photos/";
    // 创建文件名
    QString fileName = path + dateTimeString + ".png";
    QImageWriter writer;
    writer.setFileName(fileName); // 使用基于当前时间的文件名
    writer.setFormat("png"); // 设置保存格式为PNG

    QDir saveDir(path);
    if (!saveDir.exists()) {
        if (!saveDir.mkpath(path)) {
            qDebug() << "Failed to create directory ./save/";
            return; // 如果无法创建目录，则退出函数
        }
    }
    // 保存图像
    if (!writer.write(img)) {
        qDebug() << "Failed to save image:" << writer.errorString();
    } else {
        qDebug() << "Image saved successfully to" << fileName;
        ui->showimg->setStyleSheet("border-image:url("+fileName+")");
    }
}
// 用于展示最新保存的图片
QString MainWindow::findOldestImage(const QString &folderPath) {
    QDir dir(folderPath);
    if (!dir.exists()) {
        qDebug() << "Directory does not exist.";
        return QString();
    }

    // 列出目录中的所有文件，并过滤出图片文件
    QStringList filters;
    filters << "*.png"; // 添加更多图片格式如果需要
    dir.setNameFilters(filters);

    QFileInfoList list = dir.entryInfoList();
    if (list.isEmpty()) {
        qDebug() << "No image files found.";
        return QString();
    }

    // 初始化为列表中的第一个文件
    QFileInfo oldestImage = list.first();

    // 遍历文件信息列表，找到创建时间最早的图片
    for (int i = 1; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        if (fileInfo.created() > oldestImage.created()) {
            oldestImage = fileInfo;
        }
    }

    // 返回创建时间最早的图片的完整路径
    return oldestImage.absoluteFilePath();
}