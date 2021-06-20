project(FlashLogger)

cmake_minimum_required(VERSION 3.14)

include(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()
set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_FLAGS "-pg") - use only with GCC 's own profiler

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "default install path" FORCE)
endif()

option(TESTS "Enable unit-tests" ON)
option(MICROSERVICE "Enable microservice for logging" OFF)

#boost C++
find_package(Boost COMPONENTS program_options REQUIRED)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# gRPC
if(MICROSERVICE)
    FetchContent_Declare(
      gRPC
      GIT_REPOSITORY https://github.com/grpc/grpc
      GIT_TAG        v1.38.0
    )
    FetchContent_MakeAvailable(gRPC)
    set(_PROTOBUF_LIBPROTOBUF libprotobuf)
    set(_REFLECTION grpc++_reflection)
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
    set(_GRPC_GRPCPP grpc++)
    if(CMAKE_CROSSCOMPILING)
      find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    else()
      set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
    endif()
endif()

# protobuf
FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_TAG        v3.17.3
  SOURCE_SUBDIR  cmake
)
set(protobuf_BUILD_TESTS OFF)
FetchContent_MakeAvailable(protobuf)

# googletest
if(TESTS)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.11.0
)
FetchContent_MakeAvailable(googletest)
endif()

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROTOBUF_INCLUDE_DIR}
)

# source files
if(MICROSERVICE)
    # Proto file
    get_filename_component(fl_proto "./flog.proto" ABSOLUTE)
    get_filename_component(fl_proto_path "${fl_proto}" PATH)
    # Generated sources
    set(fl_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/flog.pb.cc")
    set(fl_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/flog.pb.h")
    set(fl_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/flog.grpc.pb.cc")
    set(fl_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/flog.grpc.pb.h")

    add_custom_command(
          OUTPUT "${fl_proto_srcs}" "${fl_proto_hdrs}" "${fl_grpc_srcs}" "${fl_grpc_hdrs}"
          COMMAND ${_PROTOBUF_PROTOC}
          ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
            --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
            -I "${fl_proto_path}"
            --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
            "${fl_proto}"
          DEPENDS "${fl_proto}")

    # fl_grpc_proto
    add_library(fl_grpc_proto
            ${fl_grpc_srcs}
            ${fl_grpc_hdrs}
            ${fl_proto_srcs}
            ${fl_proto_hdrs})
    target_link_libraries(fl_grpc_proto
      ${_REFLECTION}
      ${_GRPC_GRPCPP}
      ${_PROTOBUF_LIBPROTOBUF})

    SET( _MICROSERVICE_HEADER_
        ${CMAKE_CURRENT_SOURCE_DIR}/include/FLogMicroServiceWritter.h)

#    if(TESTS)
#        set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
#        FetchContent_Declare(
#          FlashLoggergRPCServer
#          GIT_REPOSITORY https://github.com/trkinvincible/FlashLoggerForCpp-Server.git
#          GIT_TAG        v1.0-beta
#        )
#        FetchContent_MakeAvailable(FlashLoggergRPCServer)
#    endif(TESTS)
endif(MICROSERVICE)

SET( _SOURCES_
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

SET( _HEADER_
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FLogManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FLogCircularBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FLogLine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FLogWritter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FLogFileWritter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FLogUtilStructs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h
)
SET( _GTEST_HEADER_
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gtest.h
)

# include Definitions
#add_definitions(-DDEBUG=1)
if(MICROSERVICE)
    add_definitions(-DUSE_MICROSERVICE=1)
else()
    add_definitions(-DUSE_MICROSERVICE=0)
endif(MICROSERVICE)
#unset(MICROSERVICE CACHE)
if(TESTS)
    add_definitions(-DTEST_MODE=1)
else()
    add_definitions(-DTEST_MODE=0)
endif(TESTS)

# link Directories
link_directories(
    ${CMAKE_INSTALL_PREFIX}/lib
    ${Boost_LIBRARY_DIRS}
)

# Create executable
if(MICROSERVICE)
    set(LINK_LIBRARIES ${Boost_LIBRARIES} grpc++)
else()
    set(LINK_LIBRARIES protobuf ${Boost_LIBRARIES})
endif(MICROSERVICE)

if(TESTS)
    message("Rk: building EXE")
    if(MICROSERVICE)
        add_executable(${PROJECT_NAME}_EXE ${_SOURCES_} ${_HEADER_} ${_GTEST_HEADER_} ${_MICROSERVICE_HEADER_})
        target_link_libraries(${PROJECT_NAME}_EXE
            -lpthread
            -ltcmalloc
            -latomic
            -Wl,--no-as-needed -lprofiler
            -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
            ${LINK_LIBRARIES}
            gtest
            fl_grpc_proto
            ${_REFLECTION}
            ${_GRPC_GRPCPP}
            ${_PROTOBUF_LIBPROTOBUF})
    else()
        add_executable(${PROJECT_NAME}_EXE ${_SOURCES_} ${_HEADER_} ${_GTEST_HEADER_})
        target_link_libraries(${PROJECT_NAME}_EXE
            -lpthread
            -ltcmalloc
            -latomic
            -Wl,--no-as-needed -lprofiler
            -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
            gtest
            ${LINK_LIBRARIES})
    endif(MICROSERVICE)

else()
    message("Rk: building library")
    add_library(FlashLogger INTERFACE)
    target_include_directories(FlashLogger
        INTERFACE
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/FlashLoggerConfigVersion.cmake"
        VERSION 0.1
        COMPATIBILITY AnyNewerVersion
    )

    install(TARGETS FlashLogger
        EXPORT FlashLogger
        LIBRARY DESTINATION lib COMPONENT Runtime
        ARCHIVE DESTINATION lib COMPONENT Development
        RUNTIME DESTINATION bin COMPONENT Runtime
        PUBLIC_HEADER DESTINATION include COMPONENT Development
        BUNDLE DESTINATION bin COMPONENT Runtime
    )

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/FlashLoggerConfig.cmake"
        "${PROJECT_BINARY_DIR}/cmake/FlashLoggerConfig.cmake"
        INSTALL_DESTINATION lib/cmake/FlashLogger
    )

#    install(EXPORT FlashLogger DESTINATION lib/cmake/FlashLogger)
    install(FILES "${PROJECT_BINARY_DIR}/FlashLoggerConfigVersion.cmake"
                  "${PROJECT_BINARY_DIR}/FlashLoggerConfig.cmake"
            DESTINATION lib/cmake/FlashLogger)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include DESTINATION include/FlashLogger
            FILES_MATCHING PATTERN "*.h")
endif()
