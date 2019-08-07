#include "multi_thread_video_writer.h"

// UMatWithMutex::UMatWithMutex(UMatPtr &p){
//     ptr = std::move(p);
// }



int MultiThreadVideoData::push(UMatPtr &p){
    std::lock_guard<std::mutex> lock(mutex);
    data.emplace(std::move(p));
    return 0;
}

int MultiThreadVideoData::get(UMatPtr &p){
    std::lock_guard<std::mutex> lock(mutex);
    p = std::move(data.front());
    return 0;
}

int MultiThreadVideoData::pop(){
    std::lock_guard<std::mutex> lock(mutex);
    // data.front()->~UMat();
    data.pop();
    std::cout << "data size:" << data.size() << std::endl;
    return 0;
}

bool MultiThreadVideoData::empty(){
    std::lock_guard<std::mutex> lock(mutex);
    return !data.size();
}

MultiThreadVideoWriter::MultiThreadVideoWriter(std::string output_pass, Video &video_param)
{
    struct stat st;
    if (!stat(output_pass.c_str(), &st))
    {
        std::cerr << "Output file already exist." << std::endl;
        throw "Output file already exist.";
    }

    video_writer = cv::VideoWriter(output_pass, cv::VideoWriter::fourcc('F', 'M', 'P', '4'), video_param.getFrequency(), cv::Size(video_param.camera_info->width_, video_param.camera_info->height_), true);
    if (!video_writer.isOpened())
    {
        std::cerr << "Error: Can't Open Video Writer." << std::endl;
        throw "Error: Can't open video writer.";
    }

    is_writing = true;
    th1 = std::thread(&MultiThreadVideoWriter::videoWriterProcess, this); //スレッド起動
}

void MultiThreadVideoWriter::videoWriterProcess()
{
    cv::Mat _buf;
    while (1)
    { 
        if(!write_data_.empty()){
            UMatPtr data_to_write;
            cv::UMat bgr;
            write_data_.get(data_to_write);
             cv::cvtColor(*data_to_write, bgr, cv::COLOR_BGRA2BGR);
            video_writer << bgr;
            write_data_.pop();
        }
        // //繰り返し書き込み
        // {
        //     std::lock_guard<std::mutex> lock(mtx);
        //     //bufferにデータがあるか確認
        //     if (images.size() != 0)
        //     {
        //         //先頭をコピー
        //         _buf = images.front().clone();
        //         //先頭を削除
        //         images.pop_front();
        //     }
        //     else if (!is_writing)
        //     {
        //         return;
        //     }
        // }
        // //mutexがunlockされたあとにゆっくりvideoWriterに書き込み
        // if (!_buf.empty())
        // {
        //     cv::Mat bgr;
        //     cv::cvtColor(_buf, bgr, cv::COLOR_BGRA2BGR);
        //     video_writer << bgr;
        //     _buf = cv::Mat();
        // }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if ((!is_writing) && write_data_.empty())
        {
            return;
        }
    }
}

std::string MultiThreadVideoWriter::getOutputName(const char *source_video_name)
{
    std::string output_pass(source_video_name);
    std::string::size_type pos;

    // Get day and time
    time_t now = time(nullptr);
    const tm *lt = localtime(&now);
    std::stringstream s;
    s << "20";
    s << lt->tm_year - 100;
    s << "-";
    s << lt->tm_mon + 1;
    s << "-";
    s << lt->tm_mday;
    s << ".";
    s << lt->tm_hour;
    s << "-";
    s << lt->tm_min;
    s << "-";
    s << lt->tm_sec;

    if ((pos = output_pass.find_last_of(".")) == std::string::npos)
    {
        output_pass = output_pass + "_stabilized_" + s.str() + ".avi";
    }
    else
    {
        output_pass.substr(0, pos);
        output_pass = output_pass.substr(0, pos) + "_stabilized_" + s.str() + ".avi";
    }
    return output_pass;
}

void MultiThreadVideoWriter::addFrame(UMatPtr &image)
{
    
    // std::lock_guard<std::mutex> lock(mtx);
    // images.push_back(cv::Mat());
    // images.back() = image.clone();
}

// void MultiThreadVideoWriter::beginThread(){

// }

MultiThreadVideoWriter::~MultiThreadVideoWriter()
{
    join();
}

void MultiThreadVideoWriter::join()
{
    std::cout << "Multi thread video writer : Terminating..." << std::endl;
    is_writing = false;
    th1.join();
    std::cout << "Multi thread video writer : Done." << std::endl;
}