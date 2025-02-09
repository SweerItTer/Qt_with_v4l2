# 项目介绍
  基于Qt v4l2 开发的适用于RV1126Linux开发板的相机程序

## 项目进度
  在WSL(开发环境)上已经实现基本功能并且可以较为流畅运行
  但是在实际部署平台则遇到问题:UI更新图像过慢导致卡顿(UI界面),有待优化
### 优化方向:
 - [ ] 1.对于多平面摄像头,可以使用线程池来多线程处理
 - [x] 2.改用libyuv方式优化手动YUYV2RGB的方式
 - [x] 3.直接操作QImage的bits()指针，避免逐像素调用setPixel：
    ```cpp
    QImage image(w, h, QImage::Format_RGB888);
    unsigned char *dst = image.bits();
    for (...) {
        // 计算RGB值后直接写入dst指针
        *dst++ = r; *dst++ = g; *dst++ = b;
    }
    ```
 - [ ] 5.RV1126的硬件加速