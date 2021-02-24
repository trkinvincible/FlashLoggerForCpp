#include <iostream>

#include <iostream>
#include <type_traits>
#include <string>

#include "config.h"
#include "gtest.h"
#include "FLogManager.h"

LEVEL FLogManager::mCurrentLevel = LEVEL::WARN;
GRANULARITY FLogManager::mCurrentGranularity = GRANULARITY::FULL;

int main(int argc, char *argv[])
{
    //Input: FlashLogger <size_of_ring_buffer> <log_file_path> <log_file_name>
    FLogConfig config([](flashlogger_config_data &d, boost::program_options::options_description &desc){
        desc.add_options()
                ("cache.size_of_ring_buffer", boost::program_options::value<short>(&d.size_of_ring_buffer)->default_value(5), "size of buffer to log asyncoronously")
                ("cache.log_file_path", boost::program_options::value<std::string>(&d.log_file_path)->default_value("../FlashLogger"), "log file path")
                ("cache.log_file_name", boost::program_options::value<std::string>(&d.log_file_name)->default_value("flashlog.txt"), "log file name")
                ("cache.run_test", boost::program_options::value<short>(&d.run_test)->default_value(1), "choose to run test");
    });

    try {

        config.parse(argc, argv);
    }
    catch(std::exception const& e) {

        std::cout << e.what();
        return 0;
    }

    if (!config.data().run_test){

        FLogManager::globalInstance(&config).SetCopyrightAndStartService(s_copyright);
        FLOG_INFO << "Hello World Test INFO";
        FLOG_WARN << "Hello World Test WARN";
    }else{

        RunGTest(argc, argv, config);
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));

    return 0;
}
