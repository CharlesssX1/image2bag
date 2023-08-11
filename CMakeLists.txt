cmake_minimum_required(VERSION 2.8.3)
project(rosbag_direct_write)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

find_package(catkin REQUIRED COMPONENTS cpp_common rosbag_storage roscpp_serialization)

catkin_package(
	INCLUDE_DIRS include
	CATKIN_DEPENDS cpp_common rosbag_storage roscpp_serialization
	LIBRARIES rosbag_direct_write
)

include_directories(include ${catkin_INCLUDE_DIRS})

link_directories(${PROJECT_SOURCE_DIR}/lib /home/pi/ros_catkin_ws/devel/lib)

add_library(${PROJECT_NAME}
  src/direct_bag.cpp
  src/demo_common.c
  src/utils.cpp
)

add_executable(bag_write src/main.cpp)
add_dependencies(bag_write ${PROJECT_NAME})
target_link_libraries(bag_write ${catkin_LIBRARIES} ${PROJECT_NAME} realsense2 eSDKOBS securec yaml-cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

if(CATKIN_ENABLE_TESTING)
  find_package(sensor_msgs REQUIRED)
  list(APPEND catkin_LIBRARIES ${sensor_msgs_LIBRARIES})
  include_directories(include ${catkin_INCLUDE_DIRS} ${sensor_msgs_INCLUDE_DIRS})
  # Make sure include_directories and catkin_LIBRARIES are setup before adding
  add_subdirectory(test)
endif()

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h"
)
