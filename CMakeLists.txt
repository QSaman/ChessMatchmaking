project(mm-server)
cmake_minimum_required(VERSION 3.1)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#include(EnableCpp11)

file(GLOB_RECURSE SRC_LIST src/*.c* src/*.h*)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
include(FindBoost)
# https://think-async.com/Asio/boost_asio_1_18_1/doc/html/boost_asio/using.html
find_package(Boost REQUIRED COMPONENTS program_options system thread)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(src)

#The following variable requires CMake 3.1
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

option(PACKAGE_TESTS "Build the tests" OFF)
if(PACKAGE_TESTS)
	enable_testing()
	add_library(${PROJECT_NAME}-lib ${SRC_LIST})
	target_link_libraries(${PROJECT_NAME}-lib ${Boost_LIBRARIES} Threads::Threads)
	include(GoogleTest)
	add_subdirectory(test)
endif()

################################################################################

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} Threads::Threads)

