project(%{ProjectName})
cmake_minimum_required(VERSION 2.8)

# where to look first for cmake modules, before ${CMAKE_ROOT}/modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

find_package(Cxx11 REQUIRED)
#find_package(Cxx14 REQUIRED)

find_package(Threads)
#find_package(PkgConfig)

# C++11 / C++14
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX11_FLAGS} ${CXX14_FLAGS}")

# Warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

aux_source_directory(. SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
