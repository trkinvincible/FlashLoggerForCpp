//"MIT License

//Copyright (c) 2021 Radhakrishnan Thangavel

//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:

//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.

//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

// Author: Radhakrishnan Thangavel (https://github.com/trkinvincible)

#include <iostream>

#include <iostream>
#include <type_traits>
#include <string>

#include "./include/config.h"
#include "./include/gtest.h"
#include "./include/FLogManager.h"

LEVEL FLogManager::mCurrentLevel = LEVEL::CRIT;
GRANULARITY FLogManager::mCurrentGranularity = GRANULARITY::FULL;

int main(int argc, char *argv[])
{
    //Input: FlashLogger <size_of_ring_buffer> <log_file_path> <log_file_name>
    std::unique_ptr<FLogConfig> config = std::make_unique<FLogConfig>([](flashlogger_config_data &d, boost::program_options::options_description &desc){
        desc.add_options()
                ("FlashLogger.size_of_ring_buffer", boost::program_options::value<short>(&d.size_of_ring_buffer)->default_value(50), "size of buffer to log asyncoronously")
                ("FlashLogger.log_file_path", boost::program_options::value<std::string>(&d.log_file_path)->default_value("../"), "log file path")
                ("FlashLogger.log_file_name", boost::program_options::value<std::string>(&d.log_file_name)->default_value("flashlog.txt"), "log file name")
                ("FlashLogger.run_test", boost::program_options::value<short>(&d.run_test)->default_value(1), "choose to run test")
                ("FlashLogger.server_ip", boost::program_options::value<std::string>(&d.server_ip)->default_value("localhost"), "microservice server IP")
                ("FlashLogger.server_port", boost::program_options::value<std::string>(&d.server_port)->default_value("50051"), "microservice server port");
    });

    try {

        config->parse(argc, argv);
    }
    catch(std::exception const& e) {

        std::cout << e.what();
        return 0;
    }

    if (!config->data().run_test){

        try{

            FLogManager::globalInstance(std::move(config)).SetCopyrightAndStartService(s_copyright);
        }catch(std::exception& exp){

            std::cout << __FUNCTION__ << "  FLOG service not started: " << exp.what() << std::endl;
        }
        FLOG_INFO << __FUNCTION__ << "  INFO";
        FLOG_WARN << __FUNCTION__ << "  WARN";
        FLOG_CRIT << __FUNCTION__ << "  CRIT";
    }else{

        RunGTest(argc, argv, std::move(config));
        FLogManager::globalInstance().SetLogLevel("INFO");
    }

    return 0;
}
