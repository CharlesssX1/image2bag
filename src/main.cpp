#include <string>
#include <csignal>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>
#include "rosbag_direct_write/direct_bag.h"
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>
#include <librealsense2/rs.hpp>

bool interrupted = false;

// Handle interruption signal
void sigintHandler(int signal) {
    interrupted = true;
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

int main () {
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

    std::string bag_name = getCurrentTime() + ".bag";
    // Explicitly set the chunk size to 768kb
    rosbag_direct_write::DirectBag bag(bag_name, 768 * 1024);

    // get frames from camera, write to bag until be interrupted
    while (!interrupted) {
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
    bag.close();
}