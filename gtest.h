#include "FLogManager.h"
#include <gtest/gtest.h>

TEST(FlashLoggerTest, LOG_INFO) {

    FLogManager::globalInstance().SetLogLevel("INFO");

    for(unsigned int i = 0; i < 10; i++)
        FLOG_INFO << "char*: " <<"Hello World Test WARN : "
                  << "unsigned int : " << i
                  << "signed int: " << static_cast<int>(i - 10)
                  << "double: " << static_cast<double>(i);
}
TEST(CacheManagerTest, LOG_WARNING) {

    FLogManager::globalInstance().SetLogLevel("WARN");

    FLOG_WARN << "Hello World Test WARN";
}
TEST(CacheManagerTest, LOG_CRITICAL) {

    FLogManager::globalInstance().SetLogLevel("CRIT");

    FLOG_INFO << "Hello World Test INFO";
    FLOG_CRIT << "Hello World Test CRIT";
}

int RunGTest(int argc, char **argv, FLogConfig& p_Config) {

    FLogManager::globalInstance(&p_Config);
    FLogManager::SetLogGranularity("BASIC");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
