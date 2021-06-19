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
find_package(Protobuf REQUIRED)
include_directories()

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
# include Directories
if(MICROSERVICE)
    # Proto file
    get_filename_component(fl_proto "./flog.proto" ABSOLUTE)
    get_filename_component(fl_proto_path "${fl_proto}" PATH)
    # Generated sources
    set(fl_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/flog.pb.cc")
    set(fl_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/flog.pb.h")
    set(fl_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/flog.grpc.pb.cc")
    set(fl_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/flog.grpc.pb.h")

    message("Rk:_PROTOBUF_PROTOC ${_PROTOBUF_PROTOC}")
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
        ${CMAKE_CURRENT_SOURCE_DIR}/FLogMicroServiceWritter.h)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogCircularBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogLine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogWritter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogFileWritter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FLogUtilStructs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h
)
SET( _GTEST_HEADER_
    ${CMAKE_CURRENT_SOURCE_DIR}/gtest.h
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
    ${Boost_LIBRARY_DIRS}
    ${CMAKE_INSTALL_PREFIX}/lib
)

# Create executable
if(MICROSERVICE)
    set(LINK_LIBRARIES ${Boost_LIBRARIES} grpc++ gtest)
else()
    set(LINK_LIBRARIES ${Protobuf_LIBRARIES} ${Boost_LIBRARIES} gtest)
endif(MICROSERVICE)

message("Rk: LINK_LIBRARIES: ${_PROTOBUF_LIBPROTOBUF}")
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
            ${LINK_LIBRARIES})
    endif(MICROSERVICE)

else()
    message("Rk: building .so")
    add_library(${PROJECT_NAME} INTERFACE)
    if(MICROSERVICE)
        install(FILES ${_HEADER_} ${_MICROSERVICE_HEADER_} ${CMAKE_CURRENT_SOURCE_DIR}/config.cfg
          DESTINATION include/${PROJECT_NAME}
        )
    else()
        install(FILES ${_HEADER_} ${CMAKE_CURRENT_SOURCE_DIR}/config.cfg
          DESTINATION include/${PROJECT_NAME}
        )
    endif(MICROSERVICE)

    #target_include_directories(${PROJECT_NAME} INTERFACE iinclude/${PROJECT_NAME)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}
        RUNTIME DESTINATION lib
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
     )
    install(
        EXPORT ${PROJECT_NAME}
        DESTINATION lib/cmake/${PROJECT_NAME} COMPONENT dev
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
    set(${PROJECT_NAME}_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME)
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
    target_link_libraries(${PROJECT_NAME} INTERFACE
        -lpthread
        -latomic
        ${LINK_LIBRARIES}
    )
endif()








