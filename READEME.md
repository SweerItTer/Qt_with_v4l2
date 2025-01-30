# 优化方向:
 - [ ] 1.对于多平面摄像头,可以使用线程池来多线程处理
 - [ ] 2.改用libyuv方式优化手动YUYV2RGB的方式
 - [ ] 3.直接操作QImage的bits()指针，避免逐像素调用setPixel：
    ```cpp
    QImage image(w, h, QImage::Format_RGB888);
    unsigned char *dst = image.bits();
    for (...) {
        // 计算RGB值后直接写入dst指针
        *dst++ = r; *dst++ = g; *dst++ = b;
    }
    ```
 - [ ] 4.使用SIMD指令批量处理像素(可能包含到第二点了)
   ```cpp
   #include <emmintrin.h> // SSE2

    // 示例：批量加载YUYV数据并计算
    __m128i yuyv = _mm_loadu_si128((__m128i*)data_);
   ```
 - [ ] 5.RV1126的硬件加速