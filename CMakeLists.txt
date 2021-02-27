project(FlashLogger)

cmake_minimum_required(VERSION 2.8)

include(FindProtobuf)
include(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()
set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_FLAGS "-pg") - use only with GCC 's own profiler

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})

SET( _SOURCES_

    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

SET( _HEADER_

    ${CMAKE_CURRENT_SOURCE_DIR}/FLogManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogCircularBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogLine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogFileUtility.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogUtilStructs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gtest.h
)

find_package(Boost COMPONENTS program_options REQUIRED)

######################
#Include Definitions #
######################
#add_definitions(-DDEBUG=1)

######################
#Include Directories #
######################
include_directories(${Boost_INCLUDE_DIRS})

######################
#link Directories    #
######################
link_directories()

######################
# Create executable  #
######################
add_executable(${PROJECT_NAME} ${_SOURCES_} ${_HEADER_})

target_link_libraries(${PROJECT_NAME}
    -lpthread
    -ltcmalloc
    -latomic
    -Wl,--no-as-needed -lprofiler
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    ${Boost_LIBRARIES}
    ${PROTOBUF_LIBRARY}
    ${GTEST_LIBRARIES}
)








