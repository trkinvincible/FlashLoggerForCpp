#ifndef FLOG_MANAGER_HPP
#define FLOG_MANAGER_HPP

#include <iostream>
#include <thread>
#include <type_traits>
#include <chrono>
#include <queue>
#include <mutex>
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

        mExit.store(true, std::memory_order_relaxed);
    }

    FLogManager(const FLogConfig* p_Config) noexcept
        :mConfig(p_Config),
         mFileUtility(std::string(p_Config->data().log_file_path +"/"+ p_Config->data().log_file_name)),
         mProducerThread(&FLogManager::ProducerThreadRun, this),
         mConsumerThread(&FLogManager::ConsumerThreadRun, this),
         mAsyncBuffer(new FLogCircularBuffer<FLogLine::type>(p_Config->data().size_of_ring_buffer)){

        uint noOfLogicalCores = std::thread::hardware_concurrency();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        // run producer and consumer in same processor(s) so the data cache is intact
        for(int cpu_index = 0; cpu_index < noOfLogicalCores; cpu_index += 2){
            CPU_SET(cpu_index, &cpuset);
        }
        int rc = pthread_setaffinity_np(mProducerThread.native_handle(),sizeof(cpu_set_t), &cpuset);
        rc += pthread_setaffinity_np(mConsumerThread.native_handle(),sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << std::endl;
        }

        mProducerThread.detach();
        mConsumerThread.detach();
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

        static std::mutex t;
        std::unique_lock<std::mutex> lk0(mProdMutex, std::defer_lock);
        std::unique_lock<std::mutex> lk1(t, std::defer_lock);
        std::lock(lk0, lk1);
        mProdMessageBox.push(p_Msg);
//        lk.unlock();
        if (p_Msg.isEnd)
            mProdCondVariable.notify_one();
    }

    void ProducerThreadRun(){

        std::once_flag startConsumer;
        std::unique_lock<std::mutex> lk0(mProdConsSyncMutex);
        while (true){

            if (mExit.load()) return;

            // Wait for a message to be added to the queue
            std::unique_lock<std::mutex> lk1(mProdMutex);
            mProdCondVariable.wait(lk1, [this]() {
                return !mProdMessageBox.empty() || mExit.load();
            });

            while (!mProdMessageBox.empty()){

                ProducerMsg msg = mProdMessageBox.front();
#ifdef DEBUG
                for (auto& v : msg.data){

                    std::visit([](auto&& arg){std::cout << "msd.data: " << arg << std::endl; }, v);
                }
#endif
                mProdMessageBox.pop();
                mAsyncBuffer->WriteData(std::move(msg));

                std::call_once(startConsumer, [&lk0, this](){

                    lk0.unlock();
                    mProdConsSyncCondVariable.notify_one();
                });
            }
            lk1.unlock();
        }
    }

    void ConsumerThreadRun(){

        // Let some data get logged first
        std::unique_lock<std::mutex> lk(mProdConsSyncMutex);
        mProdConsSyncCondVariable.wait(lk, [this]() {
            return !mExit.load();
        });

        while(true){

            char* start = nullptr; std::size_t end;
            if (mAsyncBuffer->ReadData(&start, end)){

                mFileUtility.WriteToFile(start, std::min(sizeof(FLogLine::type), end));
            }else{

                std::this_thread::yield();
            }

            if (mExit.load()){

                if (mAsyncBuffer->ReadData(&start, end)){

                    mFileUtility.WriteToFile(start, std::min(sizeof(FLogLine::type), end));
                }
                return;
            }
        }
    }

private:
    const FLogConfig* mConfig;
    std::unique_ptr<FLogCircularBuffer<FLogLine::type>> mAsyncBuffer;

    std::thread mProducerThread;
    std::queue<ProducerMsg> mProdMessageBox;
    std::mutex mProdMutex;
    std::condition_variable mProdCondVariable;

    std::mutex mProdConsSyncMutex;
    std::condition_variable mProdConsSyncCondVariable;

    std::thread mConsumerThread;

    // Load level from ENV variable will be make more productive
    static LEVEL mCurrentLevel;
    static GRANULARITY mCurrentGranularity;
    FileUtility mFileUtility;
    std::atomic<bool> mExit{false};
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

