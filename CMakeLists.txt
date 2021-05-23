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

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "default install path" FORCE)
endif()

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
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
 )

######################
#link Directories    #
######################
link_directories(
    ${CMAKE_INSTALL_PREFIX}/lib
)

######################
# Create executable  #
######################
#add_executable(${PROJECT_NAME} ${_SOURCES_} ${_HEADER_})
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

ExternalProject_Add(
  googletest
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/ext/googletest
  CMAKE_ARGS -DBUILD_GMOCK=ON -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/googletest -DCMAKE_INSTALL_LIBDIR=${CMAKE_BINARY_DIR}/googletest/lib
  )
include(CTest)
enable_testing()

add_library(${PROJECT_NAME} SHARED ${_SOURCES_})
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

install(FILES ${_HEADER_}
  DESTINATION include/${PROJECT_NAME}
  )

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}
  RUNTIME DESTINATION lib
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)

install(
  EXPORT ${PROJECT_NAME}
  DESTINATION lib/cmake/${PROJECT_NAME} COMPONENT dev
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  )

set(${PROJECT_NAME}_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(${PROJECT_NAME}_LIBRARIES ${PROJECT_NAME})
configure_file(
  ${PROJECT_SOURCE_DIR}/${PROJECT_NAME}.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config-install.cmake
  @ONLY
  )
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config-install.cmake
  DESTINATION lib/cmake/${PROJECT_NAME} COMPONENT dev
  RENAME ${PROJECT_NAME}Config.cmake
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  )

target_link_libraries(${PROJECT_NAME}
    -lpthread
    -ltcmalloc
    -latomic
    -Wl,--no-as-needed -lprofiler
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    ${Boost_LIBRARIES}
    ${PROTOBUF_LIBRARY}
)








