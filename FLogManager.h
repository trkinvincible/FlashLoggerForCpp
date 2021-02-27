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
#include <queue>
#include <mutex>
#include <future>
#include <condition_variable>

#include "config.h"
#include "FLogUtilStructs.h"
#include "FLogLine.h"
#include "FLogCircularBuffer.h"
#include "FLogFileUtility.h"

class FLogManager{

public:
    FLogManager(const FLogManager &rhs) = delete;
    FLogManager& operator=(const FLogManager &rhs) = delete;
    static FLogManager& globalInstance(FLogConfig* p_Config = nullptr){

        static FLogManager s_Self(p_Config);
        return s_Self;
    }

    ~FLogManager(){

        mHostAppExited.store(true, std::memory_order_relaxed);
        mTasksFutures[0].get();
        mConsExit.store(true, std::memory_order_relaxed);
        mTasksFutures[1].get();
    }

    FLogManager(const FLogConfig* p_Config) noexcept
        :mConfig(p_Config),
         mFileUtility(std::string(p_Config->data().log_file_path +"/"+ p_Config->data().log_file_name)),
         mAsyncBuffer(new FLogCircularBuffer<FLogLine::type>(p_Config->data().size_of_ring_buffer)){

        mTasksFutures.reserve(2);
        std::packaged_task<void(void)> taskProd(std::bind(&FLogManager::ProducerThreadRun, this));
        mTasksFutures.emplace_back(std::move(taskProd.get_future()));
        std::packaged_task<void(void)> taskCons(std::bind(&FLogManager::ConsumerThreadRun, this));
        mTasksFutures.emplace_back(std::move(taskCons.get_future()));

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

    void SetCopyrightAndStartService(const std::string& p_Data){

        mFileUtility.WriteToFile(p_Data.c_str(), p_Data.length());
        // Configure the essential ENV variables
        std::string val = []()->const char* { if (char* buf=::getenv("FLOG_LOG_LEVEL")) return buf; return " "; }();
        FLogManager::SetLogLevel(val);
        val = []()->const char* { if (char* buf=::getenv("FLOG_GRANULARITY")) return buf; return " "; }();
        FLogManager::SetLogGranularity(val);
    }

    static void SetLogLevel(std::string p_level){

        if (p_level.empty()) return;
        mCurrentLevel = (p_level == "INFO" ? LEVEL::INFO :
                         p_level == "WARN" ? LEVEL::WARN : LEVEL::CRIT);
    }

    static void SetLogGranularity(std::string p_granularity){

        if (p_granularity.empty()) return;
        mCurrentGranularity = (p_granularity == "BASIC" ? GRANULARITY::BASIC : GRANULARITY::FULL);
    }

    static bool toLog(LEVEL p_level) {

        return (mCurrentLevel >= p_level);
    }

    static bool IsFull() {

        return (mCurrentGranularity == GRANULARITY::FULL);
    }

    friend void AddProdMsgExternal(ProducerMsg&& p_Msg);
    void AddProdMsg(const ProducerMsg&& p_Msg){

        static volatile uint s_count = 0;
        if (mProdMutex.try_lock()){

            ++s_count;
            mProdMessageBox.push(p_Msg);
        }
        if (p_Msg.isEnd){

            ++s_count;
            while(--s_count) mProdMutex.unlock();
            mProdCondVariable.notify_one();
        }
    }

    void ProducerThreadRun(){

        std::once_flag startConsumer;
        while (true){

            // Wait for a message to be added to the queue
            std::unique_lock<std::recursive_mutex> lk1(mProdMutex, std::defer_lock);
            if (mProdMessageBox.empty()){

                lk1.lock();
                if (!mProdCondVariable.wait_for(lk1, 5s, [this]() {
                    return !mProdMessageBox.empty() && mHostAppExited.load();
                })){
                    return;
                }
            }

            while (!mProdMessageBox.empty()){

                ProducerMsg msg(mProdMessageBox.front());
                while (!mAsyncBuffer->WriteData(std::move(msg))){

                    std::this_thread::sleep_for(70ms);
                    ProducerMsg tmp = mProdMessageBox.front();
                    msg = tmp;
                }
                mProdMessageBox.pop();

                std::call_once(startConsumer, [this](){

                    mStartReader.store(true);
                });
            }
            lk1.unlock();
        }
    }

    void ConsumerThreadRun(){

        while(true){

            // Let some data get logged first
            if (mStartReader.load() == false)
                std::this_thread::sleep_for(10s);

            if (mConsExit.load()){

                char* start = nullptr; std::size_t end;
                while (!mAsyncBuffer->FlushBuffer(&start, end)){

                    if (end != 0){

                        mFileUtility.WriteToFile(start, std::min(sizeof(FLogLine::type), end));
                    }
                }

                return;
            }

            char* start = nullptr; std::size_t end, pos;
            if (mAsyncBuffer->ReadData(&start, end, pos)){

                mFileUtility.WriteToFile(start, std::min(sizeof(FLogLine::type), end));
                mAsyncBuffer->UnlockReadPos(pos);
            }else{

                std::this_thread::sleep_for(100ms);
            }
        }
    }

private:
    const FLogConfig* mConfig;
    std::unique_ptr<FLogCircularBuffer<FLogLine::type>> mAsyncBuffer;

    std::thread mProducerThread;
    std::queue<ProducerMsg> mProdMessageBox;
    std::recursive_mutex mProdMutex;
    std::condition_variable_any mProdCondVariable;

    std::thread mConsumerThread;
    std::vector<std::future<void>> mTasksFutures;

    // Load level from ENV variable will be make more productive
    static LEVEL mCurrentLevel;
    static GRANULARITY mCurrentGranularity;
    FileUtility mFileUtility;
    std::atomic<bool> mHostAppExited{false};
    std::atomic<bool> mConsExit{false};
    std::atomic<bool> mStartReader{false};

};

void AddProdMsgExternal(ProducerMsg&& p_Msg){

    FLogManager::globalInstance().AddProdMsg(std::move(p_Msg));
}

inline uint64_t FLogNow(){

    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

struct FLogLineDummy : public FLogLine{

    const FLogLineDummy& operator<<(const var_t&& p_Arg) const override{

        std::cout << "ignored: " << std::endl;
    }
};

#endif /* FLOG_MANAGER_HPP */

