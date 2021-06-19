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

#include "config.h"
#include "gtest.h"
#include "FLogManager.h"

LEVEL FLogManager::mCurrentLevel = LEVEL::CRIT;
GRANULARITY FLogManager::mCurrentGranularity = GRANULARITY::FULL;

int main(int argc, char *argv[])
{
    //Input: FlashLogger <size_of_ring_buffer> <log_file_path> <log_file_name>
    FLogConfig config([](flashlogger_config_data &d, boost::program_options::options_description &desc){
        desc.add_options()
                ("cache.size_of_ring_buffer", boost::program_options::value<short>(&d.size_of_ring_buffer)->default_value(5), "size of buffer to log asyncoronously")
                ("cache.log_file_path", boost::program_options::value<std::string>(&d.log_file_path)->default_value("../"), "log file path")
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

        try{

            FLogManager::globalInstance(&config).SetCopyrightAndStartService(s_copyright);
        }catch(std::exception& exp){

            std::cout << __FUNCTION__ << "  FLOG service not started: " << exp.what() << std::endl;
        }
        FLOG_INFO << __FUNCTION__ << "  INFO";
        FLOG_WARN << __FUNCTION__ << "  WARN";
        FLOG_CRIT << __FUNCTION__ << "  CRIT";
    }else{

        RunGTest(argc, argv, config);
    }

    return 0;
}
