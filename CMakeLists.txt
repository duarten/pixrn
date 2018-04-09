cmake_minimum_required(VERSION 3.6)
project(pixrn)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Werror")

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      OFF)
find_package(Boost 1.66.0 COMPONENTS filesystem REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories("/usr/local/include") 

link_directories("/usr/local/lib")

set(SOURCE_FILES main.cc)
add_executable(pixrn ${SOURCE_FILES})
target_link_libraries(pixrn ${Boost_LIBRARIES} "exif")
