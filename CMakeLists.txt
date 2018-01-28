project(flightManager)

SET(CROSS_TOOLCHAIN_PATH /home/ludger/Developer/raspi-tools)

set(CMAKE_C_STANDARD 11)
# Define our host system
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

# Define the cross compiler locations
SET(CMAKE_C_COMPILER   ${CROSS_TOOLCHAIN_PATH}/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER ${CROSS_TOOLCHAIN_PATH}/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc)

# Define the sysroot path for the RaspberryPi distribution in our tools folder
SET(CMAKE_FIND_ROOT_PATH ${CROSS_TOOLCHAIN_PATH}/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/)

# Use our definitions for compiler tools
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories only
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

add_definitions(-Wall -Wextra)

cmake_minimum_required(VERSION 3.0)

# Have CMake find our pthreads library within our toolchain (required for this library)
#set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
#find_package(Threads REQUIRED)

add_custom_target(
        createProtobuf
        COMMAND protoc-c ${CMAKE_CURRENT_SOURCE_DIR}/../communicationProtocol.proto --c_out=${CMAKE_CURRENT_SOURCE_DIR}/protobuf -I ${CMAKE_CURRENT_SOURCE_DIR}/../
)

#The libserial library
add_library(libserialport STATIC  libserialport/serialport.c libserialport/linux.c libserialport/linux_termios.c libserialport/config.h)
target_include_directories(libserialport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libserialport)

#The main executable
add_executable(flightManager main.c protobuf/communicationProtocol.pb-c.c serialPort.c database.c database.h databaseCommands.h)
add_dependencies(flightManager createProtobuf)
find_library (PROTOBUF-C protobuf-c ${CROSS_TOOLCHAIN_PATH}/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/usr/lib/arm-linux-gnueabihf)
find_library(SQLITE3 sqlite3 ${CROSS_TOOLCHAIN_PATH}/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf/arm-linux-gnueabihf/sysroot/usr/lib/arm-linux-gnueabihf)
target_link_libraries(flightManager ${PROTOBUF-C} m libserialport pthread ${SQLITE3} rt)
