# FlashLoggerForCpp

## Logger for ultra low latency in C++ (https://github.com/trkinvincible/FlashLoggerForCpp):

Open Sourced under MIT license. This project implements ultra fast logging for CPP projects on LINUX. 
This logger **guarantees both _latency_ in microseconds and high _availability_**. 
Easy to integrate into both **_monolithic_ and _microservices_ applications** 
Improved Latency with aligned and continuous memory access, instruction and data cache intact in same processor(s), 
**ProtoBuf for zero copy and zero allocation to move data from in-memory to external IO**. 

## Steps to integrate as seperate microservice
1. Deploy the [gRPC](https://github.com/grpc/grpc) powered C++ [server impl](https://github.com/trkinvincible/FlashLoggerForCpp-Server) 
in NGINX web server ![NGINX](https://www.nginx.com/wp-content/uploads/2018/03/gRPC-nginx-proxy.png)Image courtesy of NGINX  ref: [SETUP](https://www.nginx.com/blog/nginx-1-13-10-grpc/)
for **_load_ _balancing_ , _security_, _API gateway_ and reverse proxy routing**
2. Host the webserver in [NGINX docker](https://hub.docker.com/_/nginx) or any cloud 

## Steps to integrate 

1. CMakeLists.txt

``` cmake
set(TESTS OFF CACHE INTERNAL "")
set(MICROSERVICE OFF CACHE INTERNAL "")

include(FetchContent)
find_library(FLASHLOGGER_LIB
  NAMES FlashLogger
)
if(NOT FLASHLOGGER_LIB)
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(
      FlashLogger
      GIT_REPOSITORY https://github.com/trkinvincible/FlashLoggerForCpp.git
      GIT_TAG        v1.3
    )
    FetchContent_MakeAvailable(FlashLogger)
endif()

if(MICROSERVICE)
    target_link_libraries(${PROJECT_NAME}
        -lpthread
        -latomic
        FlashLogger
        ${Boost_LIBRARIES} 
        grpc++
        fl_grpc_proto
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF})
else()
    target_link_libraries(${PROJECT_NAME}
        -lpthread
        -latomic
        FlashLogger
        protobuf 
        ${Boost_LIBRARIES})
endif(MICROSERVICE)
```
2. Source File
```c++
#include <FLogManager.h>
#include <config.h>

FLogConfig config([](flashlogger_config_data &d, boost::program_options::options_description &desc){
        desc.add_options()
        ("FlashLogger.size_of_ring_buffer", boost::program_options::value<short>(&d.size_of_ring_buffer)->default_value(5), "size of buffer to log")
        ("FlashLogger.log_file_path", boost::program_options::value<std::string>(&d.log_file_path)->default_value("../"), "log file path")
        ("FlashLogger.log_file_name", boost::program_options::value<std::string>(&d.log_file_name)->default_value("flashlog.txt"), "log file name")
        ("FlashLogger.run_test", boost::program_options::value<short>(&d.run_test)->default_value(0), "choose to run test")
        ("FlashLogger.server_ip", boost::program_options::value<std::string>(&d.server_ip)->default_value("localhost"), "microservice server IP")
        ("FlashLogger.server_port", boost::program_options::value<std::string>(&d.server_port)->default_value("50051"), "microservice server port");
});

try {

    config.parse(argc, argv);
}
catch(std::exception const& e) {

    std::cout << e.what();
    return 0;
}

FLogManager::globalInstance(&config).SetCopyrightAndStartService(s_copyright);

FLogManager::globalInstance().SetLogLevel("INFO");

FLOG_INFO << __FUNCTION__ << "  INFO";
FLOG_WARN << __FUNCTION__ << "  WARN";
FLOG_CRIT << __FUNCTION__ << "  CRIT";
```
```diff
+ particularly designed to search & replace std::cout with FLOG_INFO/FLOG_WARN/FLOG_CRIT 
...

