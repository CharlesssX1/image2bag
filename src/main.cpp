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
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>
#include <librealsense2/rs.hpp>
#include <OBS/cJSON.h>
//#include <OBS/eSDKOBS.h>
//#include <OBS/securec.h>
#include <OBS/securectype.h>
#include <OBS/demo_common.h>

// bool interrupted = false;
std::queue<std::string> waitingList;
rosbag_direct_write::DirectBagCollection bag;
std::mutex mtx;
std::condition_variable cv;
bool stopFlag = false;
std::string folder_name;
int callback_num = 0;

uint64_t open_file_and_get_length(char *localfile, put_file_object_callback_data *data)
{
    uint64_t content_length = 0;
    const char *body = 0;
    if (!content_length) 
    {
        struct stat statbuf;
        if (stat(localfile, &statbuf) == -1)
        {
            fprintf(stderr, "\nERROR: Failed to stat file %s: ",
            localfile);
            perror(0);
            exit(-1);
        }
        content_length = statbuf.st_size;
    }
    if (!(data->infile = fopen(localfile, "rb"))) 
    {
        fprintf(stderr, "\nERROR: Failed to open input file %s: ",
        localfile);
        perror(0);
        exit(-1);
    }    
    data->content_length = content_length;
    return content_length;
}

obs_status response_properties_callback(const obs_response_properties *properties, void *callback_data)
{

    if (properties == NULL)
    {
        printf("error! obs_response_properties is null!");
        if(callback_data != NULL)
        {
            obs_sever_callback_data *data = (obs_sever_callback_data *)callback_data;
            printf("server_callback buf is %s ,len is %d",
                data->buffer, data->buffer_len);
            return OBS_STATUS_OK;
        }else {
            printf("error! obs_sever_callback_data is null!");
            return OBS_STATUS_OK;
        }
    }

    if (!showResponsePropertiesG) {
        return OBS_STATUS_OK;
    }

#define print_nonnull(name, field)                                 \
    do {                                                           \
        if (properties-> field) {                                  \
            printf("%s: %s\n", name, properties-> field);          \
        }                                                          \
    } while (0)
    
    print_nonnull("ETag", etag);
    print_nonnull("expiration", expiration);
    print_nonnull("website_redirect_location", website_redirect_location);
    print_nonnull("version_id", version_id);
    print_nonnull("storage_class", storage_class);
    if (properties->last_modified > 0) {
        char timebuf[256] = {0};
        time_t t = (time_t) properties->last_modified;
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
        printf("Last-Modified: %s\n", timebuf);
    }
    int i;
    for (i = 0; i < properties->meta_data_count; i++) {
        printf("x-amz-meta-%s: %s\n", properties->meta_data[i].name,
               properties->meta_data[i].value);
    }
    return OBS_STATUS_OK;
}

void put_file_complete_callback(obs_status status,
                                     const obs_error_details *error, 
                                     void *callback_data)
{
    put_file_object_callback_data *data = (put_file_object_callback_data *)callback_data;
    data->ret_status = status;
}

int put_file_data_callback(int buffer_size, char *buffer,
                                 void *callback_data)
{
    put_file_object_callback_data *data = 
        (put_file_object_callback_data *) callback_data;
    
    int ret = 0;
    if (data->content_length) {
        int toRead = ((data->content_length > (unsigned) buffer_size) ?
                    (unsigned) buffer_size : data->content_length);
        ret = fread(buffer, 1, toRead, data->infile);
    }

    uint64_t originalContentLength = data->content_length;
    data->content_length -= ret;
    
    callback_num++;
    if (data->content_length && callback_num % 100 == 0) {
        printf("%llu bytes remaining ", (unsigned long long)data->content_length);
        printf("(%d%% complete) ...\n",
             (int)(((originalContentLength - data->content_length) * 100) / originalContentLength));
        callback_num = 0;
    }

    return ret;
}

// Handle interruption signal
void sigintHandler(int signal) {
    auto bag_files = bag.close();
    std::cout << "====================================================" << std::endl;
    std::cout << "Recording finish, recorded " << bag_files.size() << "bags in total:" << std::endl;
    for (int i = 0; i < bag_files.size(); i++) {
        std::cout << bag_files[i] << std::endl;
    }
    
    std::unique_lock<std::mutex> lock(mtx);
    stopFlag = true;
    cv.notify_one();
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

void put_object_from_file(std::string bagpath)
{
    // 上传对象名
    std::string bagname = bagpath;
    std::string dir = folder_name + "/";
    size_t pos = bagname.find(dir);
    if (pos != std::string::npos) {
        bagname.replace(pos, dir.length(), "");
    }
    std::string key_str = "guoxing-bag/" + bagname;
    char* key = new char[key_str.size() + 1];
    std::strcpy(key, key_str.c_str());
    std::cout << "======== key: " << key << std::endl;
    // char *key = "guoxing-bag/put_file_test.bag";
    
    // 上传的文件
    // std::string file_name_str = bagpath;
    char file_name[256];
    bagpath.copy(file_name, sizeof(file_name) - 1);
    file_name[bagpath.size()] = '\0';
    std::cout << "======== file_name: " << file_name << std::endl;
    // char file_name[256] = "./test_direct_mixed.bag";
    uint64_t content_length = 0;
    
    // 初始化option
    obs_options option;
    init_obs_options(&option);
    option.bucket_options.host_name = "obs.cn-south-1.myhuaweicloud.com";
    option.bucket_options.bucket_name = "roboartisan-script";
    option.bucket_options.access_key = "";
    option.bucket_options.secret_access_key = "";
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

void uploadBag() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, []{return !waitingList.empty() || stopFlag; });
        
        if (stopFlag) {
            break;
        }
        
        std::string cur_bag_name = waitingList.front();
        waitingList.pop();
        put_object_from_file(cur_bag_name);
    }
}

int main () {
    std::thread bagUploadThread(uploadBag);
    
    obs_status ret_status = OBS_STATUS_BUTT;
    ret_status = obs_initialize(OBS_INIT_ALL);
    if (OBS_STATUS_OK != ret_status)
    {
        printf("obs_initialize failed(%s).\n", obs_get_status_name(ret_status));
        return ret_status;
    } else {
        std::cout << "OBS initialize success" << std::endl;
    }
    
    // Set up signal handler
    std::signal(SIGINT, sigintHandler);

    // Initialize realsense camera
    rs2::context ctx;
    std::cout << "Start librealsense - " << RS2_API_VERSION_STR << std::endl;
    std::cout << "You have " << ctx.query_devices().size() << " Realsense device connected" << std::endl;

    // Query realsense devices, get first one
    auto devs = ctx.query_devices();
    rs2::device dev = devs[0];
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_COLOR, 0, 640, 480, RS2_FORMAT_BGR8, 30);

    rs2::pipeline pipe;
    pipe.start(cfg);

    folder_name = "/home/pi/bag_recording";
    bag.open_directory(folder_name, true, "", 4096, 100000 * 1024, 0);
    
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
