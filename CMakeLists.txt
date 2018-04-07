project(serialConnector)

set(CMAKE_C_STANDARD 11)

add_definitions(-Wall -Wextra)

cmake_minimum_required(VERSION 3.6)

add_custom_target(
        createProtobuf
        COMMAND protoc-c ${CMAKE_CURRENT_SOURCE_DIR}/../communicationProtocol.proto --c_out=${CMAKE_CURRENT_SOURCE_DIR}/protobuf -I ${CMAKE_CURRENT_SOURCE_DIR}/../
)

#The libserial library
add_library(libserialport STATIC  libserialport/serialport.c libserialport/linux.c libserialport/linux_termios.c libserialport/config.h)
target_include_directories(libserialport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libserialport)

#The main executable
add_executable(serialConnector main.c protobuf/communicationProtocol.pb-c.c serialPort.c database.c)
add_dependencies(serialConnector createProtobuf)
target_link_libraries(serialConnector protobuf-c m libserialport pthread rt sqlite3)
