#include <android/bitmap.h>
#include <android/log.h>
#include <jni.h>
#include <string>
#include <vector>
#include <cstring>

// ncnn
#include "net.h"
#include "recognize.h"
#include "mtcnn.h"

using namespace Face;

#define TAG "MtcnnSo"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
static MTCNN *mDetect;
static Recognize *mRecognize;

//sdk是否初始化成功
bool detection_sdk_init_ok = false;


extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_DicHAc_demo_Face_FaceDetectionModelInit(JNIEnv *env, jobject instance,
                                                 jstring faceDetectionModelPath_) {
    LOGD("JNI开始人脸检测模型初始化");
    //如果已初始化则直接返回
    if (detection_sdk_init_ok) {
        //  LOGD("人脸检测模型已经导入");
        return true;
    }
    jboolean tRet = false;
    if (NULL == faceDetectionModelPath_) {
        //   LOGD("导入的人脸检测的目录为空");
        return tRet;
    }

    //获取MTCNN模型的绝对路径的目录（不是/aaa/bbb.bin这样的路径，是/aaa/)
    const char *faceDetectionModelPath = env->GetStringUTFChars(faceDetectionModelPath_, 0);
    if (NULL == faceDetectionModelPath) {
        return tRet;
    }

    string tFaceModelDir = faceDetectionModelPath;
    string tLastChar = tFaceModelDir.substr(tFaceModelDir.length() - 1, 1);
    //LOGD("init, tFaceModelDir last =%s", tLastChar.c_str());
    //目录补齐/
    if ("\\" == tLastChar) {
        tFaceModelDir = tFaceModelDir.substr(0, tFaceModelDir.length() - 1) + "/";
    } else if (tLastChar != "/") {
        tFaceModelDir += "/";
    }
    LOGD("init, tFaceModelDir=%s", tFaceModelDir.c_str());
    mDetect = new MTCNN(tFaceModelDir);
    mRecognize = new Recognize(tFaceModelDir);
    mDetect->SetMinFace(40);
    mDetect->SetNumThreads(2);    // 2线程
    mRecognize->SetThreadNum(2);

    env->ReleaseStringUTFChars(faceDetectionModelPath_, faceDetectionModelPath);
    detection_sdk_init_ok = true;
    tRet = true;
    return tRet;
}

JNIEXPORT jintArray JNICALL
Java_com_DicHAc_demo_Face_FaceDetect(JNIEnv *env, jobject instance, jbyteArray imageDate_,
                                     jint imageWidth, jint imageHeight, jint imageChannel) {
    //  LOGD("JNI开始检测人脸");
    if (!detection_sdk_init_ok) {
        LOGD("人脸检测MTCNN模型SDK未初始化，直接返回空");
        return NULL;
    }

    int tImageDateLen = env->GetArrayLength(imageDate_);
    if (imageChannel == tImageDateLen / imageWidth / imageHeight) {
        LOGD("数据宽=%d,高=%d,通道=%d", imageWidth, imageHeight, imageChannel);
    } else {
        LOGD("数据长宽高通道不匹配，直接返回空");
        return NULL;
    }

    jbyte *imageDate = env->GetByteArrayElements(imageDate_, NULL);
    if (NULL == imageDate) {
        LOGD("导入数据为空，直接返回空");
        env->ReleaseByteArrayElements(imageDate_, imageDate, 0);
        return NULL;
    }

    if (imageWidth < 20 || imageHeight < 20) {
        LOGD("导入数据的宽和高小于20，直接返回空");
        env->ReleaseByteArrayElements(imageDate_, imageDate, 0);
        return NULL;
    }

    if (3 == imageChannel || 4 == imageChannel) {
        //图像通道数只能是3或4；
    } else {
        LOGD("图像通道数只能是3或4，直接返回空");
        env->ReleaseByteArrayElements(imageDate_, imageDate, 0);
        return NULL;
    }

    //int32_t minFaceSize=40;
    //mtcnn->SetMinFace(minFaceSize);

    unsigned char *faceImageCharDate = (unsigned char *) imageDate;
    ncnn::Mat ncnn_img;
    if (imageChannel == 3) {
        ncnn_img = ncnn::Mat::from_pixels(faceImageCharDate, ncnn::Mat::PIXEL_BGR2RGB,
                                          imageWidth, imageHeight);
    } else {
        ncnn_img = ncnn::Mat::from_pixels(faceImageCharDate, ncnn::Mat::PIXEL_RGBA2RGB, imageWidth,
                                          imageHeight);
    }

    std::vector<Bbox> finalBbox;
    mDetect->detect(ncnn_img, finalBbox);

    int32_t num_face = static_cast<int32_t>(finalBbox.size());
    LOGD("检测到的人脸数目：%d\n", num_face);

    int out_size = 1 + num_face * 14;
    //  LOGD("内部人脸检测完成,开始导出数据");
    int *faceInfo = new int[out_size];
    faceInfo[0] = num_face;
    for (int i = 0; i < num_face; i++) {
        faceInfo[14 * i + 1] = finalBbox[i].x1;//left
        faceInfo[14 * i + 2] = finalBbox[i].y1;//top
        faceInfo[14 * i + 3] = finalBbox[i].x2;//right
        faceInfo[14 * i + 4] = finalBbox[i].y2;//bottom
        for (int j = 0; j < 10; j++) {
            faceInfo[14 * i + 5 + j] = static_cast<int>(finalBbox[i].ppoint[j]);
        }
    }

    jintArray tFaceInfo = env->NewIntArray(out_size);
    env->SetIntArrayRegion(tFaceInfo, 0, out_size, faceInfo);
    //  LOGD("内部人脸检测完成,导出数据成功");
    delete[] faceInfo;
    env->ReleaseByteArrayElements(imageDate_, imageDate, 0);
    return tFaceInfo;
}

JNIEXPORT jintArray JNICALL
Java_com_DicHAc_demo_Face_MaxFaceDetect(JNIEnv *env, jobject instance, jbyteArray imageDate_,
                                        jint imageWidth, jint imageHeight, jint imageChannel) {
    //  LOGD("JNI开始检测人脸");
    if (!detection_sdk_init_ok) {
        LOGD("人脸检测MTCNN模型SDK未初始化，直接返回空");
        return NULL;
    }

    int tImageDateLen = env->GetArrayLength(imageDate_);
    if (imageChannel == tImageDateLen / imageWidth / imageHeight) {
        LOGD("数据宽=%d,高=%d,通道=%d", imageWidth, imageHeight, imageChannel);
    } else {
        LOGD("数据长宽高通道不匹配，直接返回空");
        return NULL;
    }

    jbyte *imageDate = env->GetByteArrayElements(imageDate_, NULL);
    if (NULL == imageDate) {
        LOGD("导入数据为空，直接返回空");
        env->ReleaseByteArrayElements(imageDate_, imageDate, 0);
        return NULL;
    }

    if (imageWidth < 20 || imageHeight < 20) {
        LOGD("导入数据的宽和高小于20，直接返回空");
        env->ReleaseByteArrayElements(imageDate_, imageDate, 0);
        return NULL;
    }

    if (3 == imageChannel || 4 == imageChannel) {
        //图像通道数只能是3或4；
    } else {
        LOGD("图像通道数只能是3或4，直接返回空");
        env->ReleaseByteArrayElements(imageDate_, imageDate, 0);
        return NULL;
    }

    //int32_t minFaceSize=40;
    //mtcnn->SetMinFace(minFaceSize);

    unsigned char *faceImageCharDate = (unsigned char *) imageDate;
    ncnn::Mat ncnn_img;
    if (imageChannel == 3) {
        ncnn_img = ncnn::Mat::from_pixels(faceImageCharDate, ncnn::Mat::PIXEL_BGR2RGB,
                                          imageWidth, imageHeight);
    } else {
        ncnn_img = ncnn::Mat::from_pixels(faceImageCharDate, ncnn::Mat::PIXEL_RGBA2RGB, imageWidth,
                                          imageHeight);
    }

    std::vector<Bbox> finalBbox;
    mDetect->detectMaxFace(ncnn_img, finalBbox);

    int32_t num_face = static_cast<int32_t>(finalBbox.size());
    LOGD("检测到的人脸数目：%d\n", num_face);

    int out_size = 1 + num_face * 14;
    //  LOGD("内部人脸检测完成,开始导出数据");
    int *faceInfo = new int[out_size];
    faceInfo[0] = num_face;
    for (int i = 0; i < num_face; i++) {
        faceInfo[14 * i + 1] = finalBbox[i].x1;//left
        faceInfo[14 * i + 2] = finalBbox[i].y1;//top
        faceInfo[14 * i + 3] = finalBbox[i].x2;//right
        faceInfo[14 * i + 4] = finalBbox[i].y2;//bottom
        for (int j = 0; j < 10; j++) {
            faceInfo[14 * i + 5 + j] = static_cast<int>(finalBbox[i].ppoint[j]);
        }
    }

    jintArray tFaceInfo = env->NewIntArray(out_size);
    env->SetIntArrayRegion(tFaceInfo, 0, out_size, faceInfo);
    //  LOGD("内部人脸检测完成,导出数据成功");
    delete[] faceInfo;
    env->ReleaseByteArrayElements(imageDate_, imageDate, 0);
    return tFaceInfo;
}


JNIEXPORT jboolean JNICALL
Java_com_DicHAc_demo_Face_FaceDetectionModelUnInit(JNIEnv *env, jobject instance) {
    if (!detection_sdk_init_ok) {
        LOGD("人脸检测MTCNN模型已经释放过或者未初始化");
        return true;
    }
    jboolean tDetectionUnInit = false;
    delete mDetect;


    detection_sdk_init_ok = false;
    tDetectionUnInit = true;
    LOGD("人脸检测初始化锁，重新置零");
    return tDetectionUnInit;

}


JNIEXPORT jboolean JNICALL
Java_com_DicHAc_demo_Face_SetMinFaceSize(JNIEnv *env, jobject instance, jint minSize) {
    if (!detection_sdk_init_ok) {
        LOGD("人脸检测MTCNN模型SDK未初始化，直接返回");
        return false;
    }

    if (minSize <= 20) {
        minSize = 20;
    }

    mDetect->SetMinFace(minSize);
    return true;
}


JNIEXPORT jboolean JNICALL
Java_com_DicHAc_demo_Face_SetThreadsNumber(JNIEnv *env, jobject instance, jint threadsNumber) {
    if (!detection_sdk_init_ok) {
        LOGD("人脸检测MTCNN模型SDK未初始化，直接返回");
        return false;
    }

    if (threadsNumber != 1 && threadsNumber != 2 && threadsNumber != 4 && threadsNumber != 8) {
        LOGD("线程只能设置1，2，4，8");
        return false;
    }

    mDetect->SetNumThreads(threadsNumber);
    return true;
}


JNIEXPORT jboolean JNICALL
Java_com_DicHAc_demo_Face_SetTimeCount(JNIEnv *env, jobject instance, jint timeCount) {

    if (!detection_sdk_init_ok) {
        LOGD("人脸检测MTCNN模型SDK未初始化，直接返回");
        return false;
    }

    mDetect->SetTimeCount(timeCount);
    return true;

}
}


// 计算网络输出特征
extern "C"
JNIEXPORT jfloatArray JNICALL
Java_com_DicHAc_demo_Face_FaceRecognize(JNIEnv *env, jobject instance,
                                        jbyteArray faceDate1_, jint w1, jint h1,
                                        jintArray landmarks1) {

    jbyte *faceDate1 = env->GetByteArrayElements(faceDate1_, NULL);

    unsigned char *faceImageCharDate1 = (unsigned char *) faceDate1;

    jint *mtcnn_landmarks1 = env->GetIntArrayElements(landmarks1, NULL);

    int *mtcnnLandmarks1 = (int *) mtcnn_landmarks1;

    ncnn::Mat ncnn_img1 = ncnn::Mat::from_pixels(faceImageCharDate1, ncnn::Mat::PIXEL_RGBA2RGB, w1,
                                                 h1);

    //人脸对齐
    ncnn::Mat det1 = mRecognize->preprocess(ncnn_img1, mtcnnLandmarks1);

    std::vector<float> feature1;
    mRecognize->start(det1, feature1);

    jfloatArray emb = env->NewFloatArray(128);
    env->SetFloatArrayRegion(emb, 0, 128, &feature1[0]);

    env->ReleaseByteArrayElements(faceDate1_, faceDate1, 0);
    env->ReleaseIntArrayElements(landmarks1, mtcnn_landmarks1, 0);

    return emb;
}
