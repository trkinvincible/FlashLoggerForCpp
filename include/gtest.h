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

#include "FLogManager.h"
#include <gtest/gtest.h>

TEST(FlashLoggerTest, LOG_INFO) {

    FLogManager::globalInstance().SetLogLevel("INFO");

    for(unsigned int i = 0; i < 10; i++){

        FLOG_INFO << "char* : " << "Hello World!!!"
                  << "unsigned int : " << i
                  << "signed int : " << static_cast<int>(i - 10)
                  << "double : " << static_cast<double>(i);
    }
}
TEST(CacheManagerTest, LOG_WARNING) {

    FLogManager::globalInstance().SetLogLevel("WARN");

    FLOG_WARN << "Hello World Test WARN";
    FLOG_CRIT << "Hello World Test CRIT";
}
TEST(CacheManagerTest, LOG_CRITICAL) {

    FLogManager::globalInstance().SetLogLevel("CRIT");

    FLOG_INFO << "Hello World Test INFO";
    FLOG_WARN << "Hello World Test WARN";
    FLOG_CRIT << "Hello World Test CRIT";
}

int RunGTest(int argc, char **argv, FLogConfig& p_Config) {

    FLogManager& flog_service = FLogManager::globalInstance(&p_Config);
    flog_service.SetCopyrightAndStartService(s_copyright);
    FLogManager::SetLogGranularity("FULL");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
