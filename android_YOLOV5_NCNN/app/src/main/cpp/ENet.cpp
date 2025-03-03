//
// Created by WZTENG on 2020/08/28 028.
//

#include "ENet.h"
#include "SimplePose.h"
#include <android/log.h>


bool ENet::hasGPU = true;
ENet *ENet::detector = nullptr;

ENet::ENet(AAssetManager *mgr) {

    ENetsim = new ncnn::Net();
    // opt 需要在加载前设置
//    ENetsim->opt.use_vulkan_compute = ncnn::get_gpu_count() > 0;  // gpu
    ENetsim->opt.use_fp16_arithmetic = true;  // fp16运算加速
    ENetsim->load_param(mgr, "ENet_sim-opt.param");
    ENetsim->load_model(mgr, "ENet_sim-opt.bin");
//    LOGD("enet_detector");

}

ENet::~ENet() {
    delete ENetsim;
}

ncnn::Mat ENet::detect_enet(JNIEnv *env, jobject image) {
    AndroidBitmapInfo img_size;
    AndroidBitmap_getInfo(env, image, &img_size);
    ncnn::Mat in_net = ncnn::Mat::from_android_bitmap_resize(env, image, ncnn::Mat::PIXEL_RGBA2RGB, target_size_w,
                                                             target_size_h);
    float norm[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
    float mean[3] = {0, 0, 0};
    in_net.substract_mean_normalize(mean, norm);

    ncnn::Mat maskout;

    auto ex = ENetsim->create_extractor();
    ex.set_light_mode(true);
    ex.set_num_threads(4);
    hasGPU = ncnn::get_gpu_count() > 0;
    ex.set_vulkan_compute(hasGPU);
    ex.input(0, in_net);
    ex.extract("output", maskout);

    int mask_c = maskout.c;
    int mask_w = maskout.w;
    int mask_h = maskout.h;
//    LOGD("jni enet mask c:%d w:%d h:%d", mask_c, mask_w, mask_h);

    cv::Mat prediction = cv::Mat::zeros(cv::Size(mask_w, mask_h), CV_8UC1);
    ncnn::Mat chn[mask_c];
    for (int i = 0; i < mask_c; i++) {
        chn[i] = maskout.channel(i);
    }
    for (int i = 0; i < mask_h; i++) {
        const float *pChn[mask_c];
        for (int c = 0; c < mask_c; c++) {
            pChn[c] = chn[c].row(i);
        }

        auto *pCowMask = prediction.ptr<uchar>(i);

        for (int j = 0; j < mask_w; j++) {
            int maxindex = 0;
            float maxvalue = 0;
            for (int n = 0; n < mask_c; n++) {
                if (pChn[n][j] > maxvalue) {
                    maxindex = n;
                    maxvalue = pChn[n][j];
                }
            }
            pCowMask[j] = maxindex;
        }

    }

//    ncnn::Mat maskMat;
//    maskMat = ncnn::Mat::from_pixels(prediction.data, ncnn::Mat::PIXEL_GRAY, prediction.cols, prediction.rows);

    ncnn::Mat maskMat;
    maskMat = ncnn::Mat::from_pixels_resize(prediction.data, ncnn::Mat::PIXEL_GRAY,
                                            prediction.cols, prediction.rows,
                                            img_size.width, img_size.height);

//    LOGD("jni enet maskMat end w:%d h:%d", maskMat.w, maskMat.h);

    return maskMat;

}

