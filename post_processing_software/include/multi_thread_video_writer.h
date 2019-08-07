#ifndef __MULTI_THREAD_VIDEO_WRITER_H__
#define __MULTI_THREAD_VIDEO_WRITER_H__

#include <memory>
#include <mutex>
#include <thread>
#include <deque>
#include <string>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include "rotation_param.h"
#include <opencv2/opencv.hpp>
#include <deque>

#include <opencv2/opencv.hpp>
#include <memory>

using UMatPtr = std::unique_ptr<cv::UMat>;

// class UMatWithMutex
// {
// public:
//     UMatWithMutex(UMatPtr &p);
// public:
//     UMatPtr ptr;
//     std::mutex mutex;
// };

// using UMatWithMutexPtr = std::unique_ptr<UMatWithMutex>;

class MultiThreadVideoData
{
public:
    MultiThreadVideoData(){};
    int push(UMatPtr &p);
    int get(UMatPtr &p);
    int pop();
    bool empty();
private:
    std::queue<UMatPtr> data;
    std::mutex mutex;
};


class MultiThreadVideoWriter
{
public:
    MultiThreadVideoWriter(std::string output_pass, Video &video_param);
    
    // void beginThread();
    void addFrame(UMatPtr &image);
    ~MultiThreadVideoWriter();
    std::string output_name(char *source_name);
    static std::string getOutputName(const char *source_video_name);
    MultiThreadVideoData write_data_;
private:
    std::mutex mtx;
    volatile bool is_writing;
    std::deque<cv::Mat> images;
    cv::VideoWriter video_writer;
    std::thread th1;
    void join();
    void videoWriterProcess();
};

#endif //__MULTI_THREAD_VIDEO_WRITER_H__