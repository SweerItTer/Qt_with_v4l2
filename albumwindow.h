
#include <QFileDialog>
#include <QMouseEvent>
#include <QString>
#include <QObject>
#include <QWidget>
#include <QGridLayout>
#include <QDir>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QDialog>
#include <QMessageBox>
#include <QScreen>
#include <QGuiApplication>
#include <QVBoxLayout>
#include <QPixmap>
#include <QTimer>

class ClickableLabel : public QLabel {
    Q_OBJECT

signals:
    void clicked(const QString &imagePath);      // 单击信号，带图片路径
    void longPressed(const QString &imagePath);  // 长按信号，带图片路径

public:
    explicit ClickableLabel(const QString &imagePath_, QWidget *parent = nullptr)
        : QLabel(parent), isPressed(false), imagePath(imagePath_) {
        longPressTimer = new QTimer(this);
        longPressTimer->setInterval(1000); // 设置长按时间阈值为 1000 毫秒
        longPressTimer->setSingleShot(true);

        connect(longPressTimer, &QTimer::timeout, this, [this]() {
            if (isPressed) {
                emit longPressed(imagePath); // 触发长按信号，传递图片路径
            }
        });
    }
    ClickableLabel(QWidget *parent = nullptr) : QLabel(parent){
        longPressTimer = new QTimer(this);
        longPressTimer->setInterval(1000); // 设置长按时间阈值为 1000 毫秒
        longPressTimer->setSingleShot(true);

        connect(longPressTimer, &QTimer::timeout, this, [this]() {
            if (isPressed) {
                emit longPressed(imagePath); // 触发长按信号，传递图片路径
            }
        });
    }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        isPressed = true;
        longPressTimer->start(); // 开始计时
        QLabel::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (longPressTimer->isActive()) {
            longPressTimer->stop(); // 如果计时器还在运行，说明是单击而非长按
            emit clicked(imagePath); // 触发单击信号，传递图片路径
        }
        isPressed = false;           // 重置按压状态
        QLabel::mouseReleaseEvent(event);
    }

private:
    QTimer *longPressTimer; // 计时器，用于检测长按
    bool isPressed;         // 记录按压状态
    QString imagePath;      // 当前控件关联的图片路径
};


class AlbumWindow : public QWidget {
    Q_OBJECT

public:
    explicit AlbumWindow(const QString &folderPath, QWidget *parent = nullptr)
        : QWidget(parent), folderPath_(folderPath) {
        this->setAttribute(Qt::WA_DeleteOnClose, true);
        layout_ = new QGridLayout;
        layout_->setSpacing(10); // 设置图片之间的间距

        scrollArea_ = new QScrollArea(this);
        QWidget *container = new QWidget(this);
        container->setLayout(layout_);
        scrollArea_->setWidget(container);
        scrollArea_->setWidgetResizable(true); // 使得布局随窗口调整

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(scrollArea_);
        setLayout(mainLayout);

        loadImages();
    }

private:
    QString folderPath_;
    QGridLayout *layout_;
    QScrollArea *scrollArea_;
    QVector<ClickableLabel*> imageLabels; // 存储所有的图片标签
    QVector<QPushButton*> imagepbt; // 存储所有的图片标签
    void loadImages() {
        QDir dir(folderPath_);
        QStringList filters;
        filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp";
        dir.setNameFilters(filters);

        QStringList imageFiles = dir.entryList();
        int row = 0, col = 0;

        for (const QString &imageFile : imageFiles) {
            QString fullPath = dir.absoluteFilePath(imageFile);

            // 创建可点击的 QLabel
            ClickableLabel *imageLabel = new ClickableLabel(fullPath, this);
            imageLabel->setPixmap(QPixmap(fullPath).scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            imageLabel->setFixedSize(120, 120);
            imageLabel->setStyleSheet("border: 1px solid gray;");
            imageLabel->setAlignment(Qt::AlignCenter);
            imageLabel->setProperty("imagePath", fullPath); // 存储图片路径

            QPushButton *deleteButton = new QPushButton("Delete", this);
            deleteButton->setFixedSize(80, 30);
            deleteButton->setObjectName(fullPath);
            // 连接删除按钮
            connect(deleteButton, &QPushButton::clicked, this, [this, fullPath, imageLabel, deleteButton]() {
                if (QMessageBox::question(this, "Delete Image", "Are you sure you want to delete this image?") == QMessageBox::Yes) {
                    QFile::remove(fullPath);
                    layout_->removeWidget(imageLabel);
                    layout_->removeWidget(deleteButton);
                    imageLabel->deleteLater();
                    deleteButton->deleteLater();
                    imageLabels.removeOne(imageLabel); // 从保存的列表中移除
                    imagepbt.removeOne(deleteButton);
                }
            });

            // 连接图片点击事件
            connect(imageLabel, &ClickableLabel::clicked, this, &AlbumWindow::showFullImage);
            connect(imageLabel, &ClickableLabel::longPressed, this, &AlbumWindow::handleLongPress);
            // 将一组元素添加进容器
            imageLabels.push_back(imageLabel); 
            imagepbt.push_back(deleteButton);

            layout_->addWidget(imageLabel, row, col);
            layout_->addWidget(deleteButton, row + 1, col);

            col++;
            if (col >= 4) { // 每行最多显示 4 张图片
                col = 0;
                row += 2;
            }
        }
    }

    void showFullImage(const QString &imagePath) {
        ClickableLabel *fullImageLabel = new ClickableLabel(imagePath, this);
        fullImageLabel->setPixmap(QPixmap(imagePath));
        fullImageLabel->setAlignment(Qt::AlignCenter);
        fullImageLabel->setStyleSheet("background-color: black;");

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(fullImageLabel);

        QDialog *dialog = new QDialog(this, Qt::Window);
        dialog->setLayout(layout);
        dialog->showFullScreen();

        // 单击退出全屏
        connect(fullImageLabel, &ClickableLabel::clicked, dialog, &QDialog::deleteLater);
        // 长按事件处理
        connect(fullImageLabel, &ClickableLabel::longPressed, this, [this, imagePath, dialog]() {
            if (handleLongPress(imagePath)) {
                dialog->deleteLater();   // 关闭全屏窗口
            }
        });    
    }

    int handleLongPress(const QString &imagePath) {
        if (QMessageBox::question(this, "Delete Image", "Do you want to delete this image?") == QMessageBox::Yes) {
            QFile::remove(imagePath); // 删除图像文件
            // 从界面中移除该图片
            auto labelToRemove = std::find_if(imageLabels.begin(), imageLabels.end(),
                                              [&imagePath](ClickableLabel *label) { return label->property("imagePath").toString() == imagePath; });
            if (labelToRemove != imageLabels.end()) {
                layout_->removeWidget(*labelToRemove);
                (*labelToRemove)->deleteLater();
                imageLabels.erase(labelToRemove);
            }
            auto buttonToRemove = std::find_if(imagepbt.begin(), imagepbt.end(),
                                              [&imagePath](QPushButton *button) { return button->objectName() == imagePath; });
            if (buttonToRemove != imagepbt.end()) {
                layout_->removeWidget(*buttonToRemove);
                (*buttonToRemove)->deleteLater();
                imagepbt.erase(buttonToRemove);
            }
            return 1;
        }
        return 0;
    }
};
