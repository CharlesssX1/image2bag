#include <string>
#include <csignal>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "rosbag_direct_write/direct_bag.h"
#include "sensor_msgs/Image.h"
#include "sensor_msgs/image_encodings.h"
#include "librealsense2/rs.hpp"
#include "OBS/cJSON.h"
#include "OBS/securectype.h"
#include "yaml-cpp/yaml.h"
#include "utils.h"

extern "C" {
    #include "OBS/demo_common.h"
}

std::queue<std::string> waitingList;
rosbag_direct_write::DirectBagCollection bag;
std::mutex mtx;
std::condition_variable cv;
bool stopFlag = false;
std::string folder_name;
int callback_num = 0;

// Handle interruption signal
void sigintHandler(int signal) {
    auto bag_files = bag.close();
    std::cout << "====================================================" << std::endl;
    std::cout << "Recording finish, recorded " << bag_files.size() << "bags in total:" << std::endl;
    for (int i = 0; i < bag_files.size(); i++) {
        std::cout << bag_files[i] << std::endl;
    }
    
    //~ std::unique_lock<std::mutex> lock(mtx);
    //~ stopFlag = true;
    //~ cv.notify_one();
}

// Get current time, format as yymmdd-hh-mm-ss
std::string getCurrentTime() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* local_time = std::localtime(&now_time_t);
    std::ostringstream oss;
    oss << std::put_time(local_time, "%y%m%d-%H-%M-%S");

    return oss.str(); 
} 

void put_object_from_file(std::string bagpath, Config &conf) {
    // 上传对象名
    std::string bagname = bagpath;
    std::string dir = folder_name + "/";
    size_t pos = bagname.find(dir);
    if (pos != std::string::npos) {
        bagname.replace(pos, dir.length(), "");
    }
    std::string key_str = conf.obsBucketDir + "/" + bagname;
    char* key = new char[key_str.size() + 1];
    std::strcpy(key, key_str.c_str());
    std::cout << "======== key: " << key << std::endl;
    
    // 上传的文件
    char file_name[256];
    bagpath.copy(file_name, sizeof(file_name) - 1);
    file_name[bagpath.size()] = '\0';
    std::cout << "======== file_name: " << file_name << std::endl;

    uint64_t content_length = 0;
    
    // 初始化option
    obs_options option;
    init_obs_options(&option);
    option.bucket_options.host_name = const_cast<char*>(conf.obsHostName.c_str());
    option.bucket_options.bucket_name = const_cast<char*>(conf.obsBucketName.c_str());
    option.bucket_options.access_key = const_cast<char*>(conf.obsAccessKey.c_str());
    option.bucket_options.secret_access_key = const_cast<char*>(conf.obsSecretKey.c_str());
    // 初始化上传对象属性
    obs_put_properties put_properties;
    init_put_properties(&put_properties);

    // 初始化存储上传数据的结构体
    put_file_object_callback_data data;
    memset(&data, 0, sizeof(put_file_object_callback_data));
    // 打开文件，并获取文件长度
    content_length = open_file_and_get_length(file_name, &data);
    // 设置回调函数
    obs_put_object_handler putobjectHandler =
    { 
        { &response_properties_callback, &put_file_complete_callback },
        &put_file_data_callback
    };

    put_object(&option, key, content_length, &put_properties, 0, &putobjectHandler, &data);
    if (OBS_STATUS_OK == data.ret_status) {
        printf("put object from file successfully. \n");
    }
    else
    {
        printf("put object failed(%s).\n",  
               obs_get_status_name(data.ret_status));
    }
}

void uploadBag(Config &conf) {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, []{return !waitingList.empty() || stopFlag; });
        
        if (stopFlag) {
            break;
        }
        
        std::string cur_bag_name = waitingList.front();
        waitingList.pop();
        put_object_from_file(cur_bag_name, conf);
    }
}

int initOBS() {
    obs_status ret_status = OBS_STATUS_BUTT;
    ret_status = obs_initialize(OBS_INIT_ALL);
    if (OBS_STATUS_OK != ret_status)
    {
        printf("obs_initialize failed(%s).\n", obs_get_status_name(ret_status));
        return ret_status;
    } else {
        std::cout << "OBS initialize success" << std::endl;
    }
    return 0;
}

int main () {
    Config conf;
    int confRet = InitConf(conf);
    if (confRet != 0) {
        std::cout << "config init failed, please check config." << std::endl;
        return -1;
    }
    
    // init OBS
    int obsRet = initOBS();
    if (obsRet != 0) {
        return -1;
    }
    if (conf.uploadOn == true) {
        std::thread bagUploadThread([&]() {
            uploadBag(conf);
        });
    }
    
    // Set up signal handler
    std::signal(SIGINT, sigintHandler);

    // Initialize realsense camera
    rs2::context ctx;
    std::cout << "Start librealsense - " << RS2_API_VERSION_STR << std::endl;
    std::cout << "You have " << ctx.query_devices().size() << " Realsense device connected" << std::endl;
    // Query realsense devices, get first one and start pipline
    if (ctx.query_devices().size() == 0) {
        std::cerr << "no realsense device detected" << std::endl;
    }
    auto devs = ctx.query_devices();
    rs2::device dev = devs[0];
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_COLOR, 0, 640, 480, RS2_FORMAT_RGB8, 30);
    rs2::pipeline pipe;
    pipe.start(cfg);
    
    // setup bag directory
    bag.open_directory(conf.recordingLocalDir, true, "", 4096, 10000000 * 1024, 0);
    
    // get frames from camera, write to bag until be interrupted
    while (true) {
        rs2::frameset frames = pipe.wait_for_frames();
        rs2::video_frame frame = frames.get_color_frame();

        int width = frame.get_width();
        int height = frame.get_height();
        int frame_size = width * height * 3;

        std::vector<uint8_t> image_data(frame_size);
        const uint8_t* frame_data = reinterpret_cast<const uint8_t*>(frame.get_data());
        std::copy(frame_data, frame_data + frame_size, image_data.begin());

        sensor_msgs::Image image;
        image.header.stamp.fromSec(ros::WallTime::now().toSec());
        image.header.frame_id = "/camera_frame";
        image.height = height;
        image.width = width;
        image.encoding = sensor_msgs::image_encodings::RGB8;
        image.is_bigendian = true;
        image.step = width * 3;
        image.data = std::move(image_data);

        bag.write("camera", image.header.stamp, image);
    }
    
    //~ size_t number_of_iterations = 3;
    //~ size_t width = 1024, height = 768;
    //~ sensor_msgs::Image image;
    //~ image.header.stamp.fromSec(ros::WallTime::now().toSec());
    //~ image.header.frame_id = "/camera_frame";
    //~ image.height = height;
    //~ image.width = width;
    //~ image.encoding = "Encoding 1";
    //~ image.is_bigendian = true;
    //~ image.step = 1;
    //~ image.data = std::vector<uint8_t>(width * height, 0x12);
    //~ while (true)
    //~ {
        //~ image.header.stamp.fromSec(ros::WallTime::now().toSec());
        //~ bag.write("camera", image.header.stamp, image);
    //~ }
    //~ bag.close();
}
