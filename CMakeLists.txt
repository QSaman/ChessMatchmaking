project(mm-server)
cmake_minimum_required(VERSION 3.1)

set(CLIENT_EXEC_NAME "mm-client")

file(GLOB_RECURSE SERVER_SRC_LIST src/server/*.c* src/server/*.h*)
file(GLOB_RECURSE CLIENT_SRC_LIST src/client/*.c* src/client/*.h*)

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

################################################################################

add_executable(${PROJECT_NAME} ${SERVER_SRC_LIST})
add_executable(${CLIENT_EXEC_NAME} ${CLIENT_SRC_LIST})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} Threads::Threads)
target_link_libraries(${CLIENT_EXEC_NAME} ${Boost_LIBRARIES} Threads::Threads)

# Enabling C++14. Add these two lines after add_executable
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

set_property(TARGET ${CLIENT_EXEC_NAME} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${CLIENT_EXEC_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
