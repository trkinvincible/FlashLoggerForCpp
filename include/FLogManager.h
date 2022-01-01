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

#ifndef FLOG_MANAGER_HPP
#define FLOG_MANAGER_HPP

#include <iostream>
#include <thread>
#include <type_traits>
#include <chrono>
#include <deque>
#include <mutex>
#include <future>
#include <condition_variable>

#include "config.h"
#include "FLogUtilStructs.h"
#include "FLogLine.h"
#include "FLogCircularBuffer.h"
#include "FLogWritter.h"
#if(USE_MICROSERVICE)
#include "FLogMicroServiceWritter.h"
#else
#include "FLogFileWritter.h"
#endif

class FLogManager{

    static constexpr std::size_t MAX_SLOT_LEN = sizeof(FLogLine);

public:
    FLogManager(const FLogManager &rhs) = delete;
    FLogManager& operator=(const FLogManager &rhs) = delete;
    static FLogManager& globalInstance(std::unique_ptr<FLogConfig> p_Config = nullptr){

        static FLogManager s_Self(std::move(p_Config));
        return s_Self;
    }

    ~FLogManager() noexcept{

        try{

            mHostAppExited.store(true, std::memory_order_relaxed);
            AddProdMsg(ProducerMsg(true, {"\n\n******FLog completed*******"}));
            std::cout << "Producer Exit: " << std::boolalpha << mTasksFutures[0].get() << std::endl;
            mConsExit.store(true, std::memory_order_relaxed);
            std::cout << "Consumer Exit: " << std::boolalpha << mTasksFutures[1].get() << std::endl;

#if(!USE_MICROSERVICE)
                const std::string tmp = std::string("subl ") + std::string(mConfig->data().log_file_path +
                                                                           "/"+ mConfig->data().log_file_name);
                system(tmp.data());
#endif
        }catch(std::exception& exp){

            std::cout << __FUNCTION__ << " FLOG service failed to exit: "
                      << exp.what() << std::endl;
        }
    }

    FLogManager(std::unique_ptr<FLogConfig> p_Config) noexcept
      #if(USE_MICROSERVICE)
          :mWritterUtility(std::string(p_Config->data().server_ip +":"+ p_Config->data().server_port)),
      #else
          :mWritterUtility(std::string(p_Config->data().log_file_path +"/"+ p_Config->data().log_file_name)),
      #endif
         mAsyncBuffer(new FLogCircularBuffer<FLogLine>(p_Config->data().size_of_ring_buffer)){

        p_Config.swap(mConfig);
    }

    void SetCopyrightAndStartService(const std::string& p_Data){

        mWritterUtility.WriteToFile((std::uint8_t*)p_Data.c_str(), p_Data.length());
        // Configure the essential ENV variables
        std::string val = []()->const char* { if (char* buf=::getenv("FLOG_LOG_LEVEL")) return buf; return " "; }();
        FLogManager::SetLogLevel(val);
        val = []()->const char* { if (char* buf=::getenv("FLOG_GRANULARITY")) return buf; return " "; }();
        FLogManager::SetLogGranularity(val);

        mTasksFutures.reserve(2);
        std::packaged_task<bool(void)> taskProd(std::bind(&FLogManager::ProducerThreadRun, this));
        mTasksFutures.push_back(std::move(taskProd.get_future()));
        std::packaged_task<bool(void)> taskCons(std::bind(&FLogManager::ConsumerThreadRun, this));
        mTasksFutures.push_back(std::move(taskCons.get_future()));

        std::thread t1(std::move(taskProd));
        std::thread t2(std::move(taskCons));

        uint noOfLogicalCores = std::thread::hardware_concurrency();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        // run producer and consumer in same processor(s) so the data cache is intact
        for(int cpu_index = 0; cpu_index < noOfLogicalCores; cpu_index += 2){

            CPU_SET(cpu_index, &cpuset);
        }
        int rc = pthread_setaffinity_np(t1.native_handle(),sizeof(cpu_set_t), &cpuset);
        rc += pthread_setaffinity_np(t2.native_handle(),sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << std::endl;
        }

        t1.detach();
        t2.detach();
    }

    const FLogLine& getFlogLine(const LEVEL p_Level, const char* f, std::uint32_t l){
            return [p_Level, f, l]()->const FLogLine&{
              static FLogLine flog;
              static FLogLineDummy flogDummy;
              if (FLogManager::toLog(p_Level)){
                if (FLogManager::IsFull()){
                  flog.InitData(FLogNow(), f,l);
                }
                return flog;
              }
              return flogDummy;
            }();
    }

    static void SetLogLevel(std::string p_level)noexcept{

        if (p_level.empty()) return;
        mCurrentLevel = (p_level == "INFO" ? LEVEL::INFO :
                         p_level == "WARN" ? LEVEL::WARN : LEVEL::CRIT);
    }

    static void SetLogGranularity(std::string p_granularity)noexcept{

        if (p_granularity.empty()) return;
        mCurrentGranularity = (p_granularity == "BASIC" ? GRANULARITY::BASIC : GRANULARITY::FULL);
    }

    static bool toLog(LEVEL p_level)noexcept{

        return (mCurrentLevel >= p_level);
    }

    static bool IsFull()noexcept{

        return (mCurrentGranularity == GRANULARITY::FULL);
    }

    friend void AddProdMsgExternal(ProducerMsg&& p_Msg);
    void AddProdMsg(ProducerMsg&& p_Msg){

        static volatile uint s_count = 0;
        {
            std::unique_lock<std::recursive_mutex> lk(mProdMutex);
            if (!lk.owns_lock())
                mProdCondVariable.wait(lk);
        }

        mProdMutex.lock();
        ++s_count;
        mProdMessageBox.push_back(std::move(p_Msg));
        if (p_Msg.isEnd){

            static int l;
            ++s_count;
            while(--s_count) {
                mProdMutex.unlock();
            }
            mProdCondVariable.notify_one();
        }
    }

    bool ProducerThreadRun(){

        std::once_flag startConsumer;
        while (true){

            try{
                std::unique_lock<std::recursive_mutex> lk(mProdMutex);
                mProdCondVariable.wait(lk, [this](){
                    return !mProdMessageBox.empty();
                });
                while (!mProdMessageBox.empty()){
                    const auto& data = mProdMessageBox.front();
                    while(true){
                        if (!mAsyncBuffer->WriteData(data)){
                            std::this_thread::sleep_for(std::chrono::microseconds(5));
                            continue;
                        }
                        mProdMessageBox.pop_front();
                        break;
                    }
                    std::call_once(startConsumer, [this](){ mStartReader.store(true, std::memory_order_relaxed); });
                }
                lk.unlock();
                if (mHostAppExited.load(std::memory_order_relaxed)){

                    return true;
                }
                mProdCondVariable.notify_one();
            }catch(std::exception& exp){

                std::cout << "producer exception: " << exp.what();
                return false;
            }
        }
    }

    bool ConsumerThreadRun(){

        // Let some data get logged first
        if (mStartReader.load(std::memory_order_relaxed) == false)
            std::this_thread::sleep_for(std::chrono::microseconds(2));

        while(true){

            try{
                std::uint8_t* start = nullptr; std::size_t end, pos;
                if (mAsyncBuffer->ReadData(&start, end, pos)){
                    if (mWritterUtility.WriteToFile(start, std::min(MAX_SLOT_LEN, end))){
                        mAsyncBuffer->UnlockReadPos(pos);
                    }
                }else{

                    std::this_thread::sleep_for(std::chrono::microseconds(5));
                }

                if (mConsExit.load(std::memory_order_relaxed)){

                    std::uint8_t* start = nullptr; std::size_t end;
                    while (!mAsyncBuffer->FlushBuffer(&start, end)){

                        if (end != 0){

                            mWritterUtility.WriteToFile(start, std::min(MAX_SLOT_LEN, end));
                        }
                    }
                    return true;
                }

            }catch(std::exception& exp){

                std::cout << "consumer exception: " << exp.what();
                return false;
            }
        }
    }

private:
    std::unique_ptr<FLogConfig> mConfig;

    std::unique_ptr<FLogCircularBuffer<FLogLine>> mAsyncBuffer;

    std::thread mProducerThread;
    std::deque<ProducerMsg> mProdMessageBox;
    std::recursive_mutex mProdMutex;
    std::condition_variable_any mProdCondVariable;

    std::thread mConsumerThread;

    std::vector<std::future<bool>> mTasksFutures;

    static LEVEL mCurrentLevel;
    static GRANULARITY mCurrentGranularity;
#if(USE_MICROSERVICE)
    FLogWritter<FLogMicroServiceWritter> mWritterUtility;
#else
    FLogWritter<FLogFileWritter> mWritterUtility;
#endif
    std::atomic_bool mHostAppExited{false};
    std::atomic_bool mConsExit{false};
    std::atomic_bool mStartReader{false};
};

void AddProdMsgExternal(ProducerMsg&& p_Msg){

    FLogManager::globalInstance().AddProdMsg(std::forward<ProducerMsg>(p_Msg));
}

#endif /* FLOG_MANAGER_HPP */

