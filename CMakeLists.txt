# ============================================
# C项目通用CMake模板
# 使用方法：复制此文件为CMakeLists.txt，然后修改下面的配置部分
# ============================================

cmake_minimum_required(VERSION 3.10)
project(Car_Device_System VERSION 1.0.0 LANGUAGES C CXX)

# 设置C++标准（如果需要）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================
# 【需要修改的配置部分】
# ============================================

# 1. 项目基本信息
set(PROJECT_NAME "Car_Device_System")      # 项目名称
set(PROJECT_VERSION "1.0.0")               # 版本号

# 2. 目录结构（根据实际项目调整）
set(SOURCE_DIR "source")      # 源文件目录
set(INCLUDE_DIR "include")    # 头文件目录
set(CLIENT_DIR "${SOURCE_DIR}/client")    # 客户端目录
set(SERVER_DIR "${SOURCE_DIR}/server")    # 服务器目录

# 4. 依赖库（根据实际需要添加）
#find_package(Threads REQUIRED)
#将SomeLib换成第三方库
# set(DEPENDENCIES SomeLib::SomeLib Threads::Threads)
#set(DEPENDENCIES Threads::Threads)
#将Threads换成第三方库

# ============================================
# 【以下部分通常不需要修改】
# ============================================

# C标准配置
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -Wextra")
    add_definitions(-DDEBUG=1)
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall")
endif()

# 自动查找源文件
file(GLOB_RECURSE SOURCE_FILES "${SOURCE_DIR}/*.c")
file(GLOB_RECURSE HEADER_FILES "${INCLUDE_DIR}/*.h")

# 查找客户端文件
file(GLOB CLIENT_FILES "${CLIENT_DIR}/*.c")
file(GLOB SERVER_FILES "${SERVER_DIR}/*.c")

# 添加额外源文件
if(EXTRA_SOURCES)
    list(APPEND SOURCE_FILES ${EXTRA_SOURCES})
endif()

# 包含目录
include_directories(${INCLUDE_DIR})

# 依赖库
find_package(Threads REQUIRED)

# 构建共享库
add_library(car_device SHARED ${SOURCE_DIR}/Car_config.c ${SOURCE_DIR}/Car_Log.c ${SOURCE_DIR}/Car_Daemon.c)
target_link_libraries(car_device Threads::Threads)

# 构建服务器可执行文件（每个子模块一个可执行文件）
file(GLOB SERVER_FILES "${SERVER_DIR}/*.c")
foreach(server_file ${SERVER_FILES})
    get_filename_component(server_name ${server_file} NAME_WE)
    add_executable(${server_name} ${server_file})
    target_link_libraries(${server_name} car_device Threads::Threads)
endforeach()

# 构建客户端可执行文件
foreach(client_file ${CLIENT_FILES})
    get_filename_component(client_name ${client_file} NAME_WE)
    add_executable(${client_name} ${client_file})
    target_link_libraries(${client_name} car_device Threads::Threads)
endforeach()

# 日志测试程序（已移除）

# 安装规则（可选）
# install(TARGETS car_server DESTINATION bin)
# install(TARGETS car_device DESTINATION lib)

# 自定义目标：运行服务器
add_custom_target(run_server
    COMMAND ./bin/car_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS car_server
    COMMENT "Running car_server"
)

# 计算文件数量
list(LENGTH SOURCE_FILES SOURCE_COUNT)
list(LENGTH HEADER_FILES HEADER_COUNT)
list(LENGTH CLIENT_FILES CLIENT_COUNT)

message(STATUS "Configured: ${PROJECT_NAME}")
message(STATUS "  Source files: ${SOURCE_COUNT} files")
message(STATUS "  Header files: ${HEADER_COUNT} files")
message(STATUS "  Client executables: ${CLIENT_COUNT}")
message(STATUS "  Build to: ${CMAKE_BINARY_DIR}/bin")
