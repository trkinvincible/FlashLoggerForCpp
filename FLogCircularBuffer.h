#ifndef FLOG_CIRCULAR_BUFFER_HPP
#define FLOG_CIRCULAR_BUFFER_HPP

#include <iostream>
#include <cstdlib>
#include <atomic>
#include <vector>
#include <thread>

class ProducerMsg;

template<typename T>
class FLogCircularBuffer {

    static constexpr bool SLOT_LOCKED   = false;
    static constexpr bool SLOT_UNLOCKED = true;
    static constexpr int CACHELINE_SIZE{64};

public:
    FLogCircularBuffer(const std::size_t p_BufferSize)
        :mBufferSize(p_BufferSize + 1),
          mBuffer(static_cast<T*>(std::aligned_alloc(CACHELINE_SIZE, sizeof(T) * mBufferSize + 1))){

        mCurrentWriteBuffer = reinterpret_cast<char*>(&mBuffer[mWritePos.load(std::memory_order_acquire)]);
    }

    void WriteData(ProducerMsg&& p_data){

        if (mBytesWrittenInCurrentWriteBuffer == 0){

            auto pos = mWritePos.load(std::memory_order_acquire);
            mBufferStatesPerSlot[pos].first.store(SLOT_LOCKED, std::memory_order_release);
            mBufferStatesPerSlot[pos].second = 0;
        }

        for(auto& v: p_data.data) {

            std::visit([this](auto&& arg) {

                using ArgT = std::decay_t<decltype(arg)>;
                if constexpr (std::is_arithmetic_v<ArgT>){
#ifdef DEBUG
                    std::cout << "is_arithmetic_v: " << arg << std::endl;
#endif
                    std::string n = std::to_string(arg);
                    auto length = n.length();
                    std::uninitialized_copy_n(n.data(), length , (mCurrentWriteBuffer + mBytesWrittenInCurrentWriteBuffer));
                    mBytesWrittenInCurrentWriteBuffer += length;
                }else if constexpr (std::is_same_v<ArgT, const char*>){
#ifdef DEBUG
                    std::cout << "char*: " << arg << std::endl;
#endif
                    auto length = arg ? strlen(arg) : 0;
                    std::uninitialized_copy_n(arg, length, (mCurrentWriteBuffer + mBytesWrittenInCurrentWriteBuffer));
                    mBytesWrittenInCurrentWriteBuffer += length;
                }
            }, v);
        }

        if (p_data.isEnd){

#ifdef DEBUG
            std::cout << "End of Log" << std::endl;
#endif
            auto oldWritePos = mWritePos.load(std::memory_order_acquire);
            auto newWritePos = getPositionAfter(oldWritePos);
            auto len = std::distance(mCurrentWriteBuffer, mCurrentWriteBuffer + mBytesWrittenInCurrentWriteBuffer);
            mWritePos.store(newWritePos, std::memory_order_release);

            // Reset all helpers
            mBytesWrittenInCurrentWriteBuffer = 0;
            mCurrentWriteBuffer = reinterpret_cast<char*>(&mBuffer[mWritePos.load(std::memory_order_acquire)]);

            mBufferStatesPerSlot[oldWritePos].second = len;
            mBufferStatesPerSlot[oldWritePos].first.store(SLOT_UNLOCKED, std::memory_order_release);

            if (newWritePos == mReadPos.load()){
#ifdef DEBUG
                std::cout << "Data loss" << std::endl;
#endif
            }
        }
    }

    bool ReadData(char** p_Data, std::size_t& p_Length){

        if (mBufferStatesPerSlot[mReadPos.load()].first.load() == SLOT_LOCKED)
            return false;

        *p_Data = reinterpret_cast<char*>(std::addressof(mBuffer[mReadPos]));
        p_Length = mBufferStatesPerSlot[mReadPos].second;
        while(true){
            auto oldWritePos = mWritePos.load();
            auto oldReadPos = mReadPos.load();
            if (oldWritePos == oldReadPos){
                return false;
            }
            if (mReadPos.compare_exchange_strong(oldReadPos, getPositionAfter(oldReadPos)))
                return true;
        }
    }

private:
    constexpr std::size_t getPositionAfter(std::size_t pos) noexcept{

        // Implement required policy to roll buffer
        return ++pos == mBufferSize ? 0 : pos;
    }

    T* mBuffer;
    const std::size_t mBufferSize;
    std::atomic<std::size_t> mWritePos{0}, mReadPos{0};
    std::array<std::pair<std::atomic_bool, std::size_t>, 20> mBufferStatesPerSlot;

    // Helper states
    char* mCurrentWriteBuffer;
    size_t mBytesWrittenInCurrentWriteBuffer{0};
};

#endif /* FLOG_CIRCULAR_BUFFER_HPP */

