cmake_minimum_required(VERSION 3.10)

project(simple_joystick)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找SDL2库
find_package(SDL2 REQUIRED)

# 包含SDL2头文件目录
include_directories(${SDL2_INCLUDE_DIRS})

# 创建可执行文件
add_executable(simple_joystick simple_joystick.cpp)

# 链接SDL2库
target_link_libraries(simple_joystick ${SDL2_LIBRARIES} pthread)

# 设置线程库 - 解决pthread链接问题
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(simple_joystick Threads::Threads)

