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

#ifndef FLOG_CIRCULAR_BUFFER_HPP
#define FLOG_CIRCULAR_BUFFER_HPP

#include <iostream>
#include <cstdlib>
#include <atomic>
#include <vector>

class ProducerMsg;

template<typename T, std::size_t N = 20>
class FLogCircularBuffer {

    static constexpr bool SLOT_LOCKED   = true;
    static constexpr bool SLOT_UNLOCKED = false;
    static constexpr int  CACHELINE_SIZE{64};

public:
    FLogCircularBuffer(const std::size_t p_BufferSize)
        :mBufferSize(p_BufferSize + 1),
          mBuffer(static_cast<T*>(std::aligned_alloc(CACHELINE_SIZE, sizeof(T) * mBufferSize))){

        mCurrentWriteBuffer = reinterpret_cast<char*>(&mBuffer[mWritePos]);
    }

    ~FLogCircularBuffer()noexcept{

        std::free(mBuffer);
    }

    bool WriteData(const ProducerMsg& p_data){

        auto pos = mWritePos.load(std::memory_order_acquire);

        if (mBufferStatesPerSlot[pos].first.load() == SLOT_LOCKED &&
            mBufferStatesPerSlot[pos].second != 0){
            return false;
        }

        if (p_data.isEnd){

            auto oldWritePos = pos;
            auto newWritePos = getPositionAfter(oldWritePos);
            // Buffer full . Reader is not fast enough so lets postpone.
            if (newWritePos == mReadPos.load(std::memory_order_acquire)){
                return false;
            }
        }

        if (mBytesWrittenInCurrentWriteBuffer == 0){

            mBufferStatesPerSlot[pos].second = 0;
            mBufferStatesPerSlot[pos].first.store(SLOT_LOCKED, std::memory_order_release);
        }

        for(const auto& v: p_data.data) {

            std::visit([this](auto&& arg) {

                using ArgT = std::decay_t<decltype(arg)>;
                if constexpr (std::is_arithmetic_v<ArgT>){

                    std::string n = std::to_string(arg);
                    const auto length = n.length();
                    std::uninitialized_move_n(n.data(), length , (mCurrentWriteBuffer + mBytesWrittenInCurrentWriteBuffer));
                    mBytesWrittenInCurrentWriteBuffer += length;
                }else if constexpr (std::is_same_v<ArgT, const char*>){

                    auto length = arg ? strlen(arg) : 0;
                    if (length){
                        std::uninitialized_move_n(arg, length, (mCurrentWriteBuffer + mBytesWrittenInCurrentWriteBuffer));
                        mBytesWrittenInCurrentWriteBuffer += length;
                    }
                }else{
                    static_assert (always_false_v<ArgT>, "unsupported type");
                }
            }, v);
        }

        if (p_data.isEnd){

            auto oldWritePos = pos;
            auto newWritePos = getPositionAfter(oldWritePos);

            // Reset all helpers
            auto len = std::distance(mCurrentWriteBuffer, mCurrentWriteBuffer + mBytesWrittenInCurrentWriteBuffer);
            mBufferStatesPerSlot[oldWritePos].second = len;
            mBufferStatesPerSlot[oldWritePos].first.store(SLOT_UNLOCKED, std::memory_order_release);

            mWritePos.store(newWritePos, std::memory_order_release);
            mBytesWrittenInCurrentWriteBuffer = 0;
            mCurrentWriteBuffer = reinterpret_cast<char*>(&mBuffer[mWritePos]);
        }

        return true;
    }

    void UnlockReadPos(const std::size_t p_Pos)noexcept{

        mBufferStatesPerSlot[p_Pos].second = 0;
        mBufferStatesPerSlot[p_Pos].first.store(SLOT_UNLOCKED, std::memory_order_release);
    }

    bool ReadData(char** p_Data, std::size_t& p_Length, std::size_t& p_CurPos){

        auto pos = mReadPos.load(std::memory_order_acquire);

        if (mBufferStatesPerSlot[pos].first.load() == SLOT_LOCKED ||
            mBufferStatesPerSlot[pos].second == 0){
            return false;
        }

        while(true){

            auto oldReadPos = pos;
            auto oldWritePos = mWritePos.load(std::memory_order_acquire);
            if (oldWritePos == oldReadPos){
                return false;
            }

            mBufferStatesPerSlot[pos].first.store(SLOT_LOCKED, std::memory_order_release);
            p_CurPos = pos;
            *p_Data = reinterpret_cast<char*>(std::addressof(mBuffer[pos]));
            p_Length = mBufferStatesPerSlot[pos].second;

            if (mReadPos.compare_exchange_strong(oldReadPos, getPositionAfter(oldReadPos)))
                return true;
        }
    }

    bool FlushBuffer(char** p_Data, std::size_t& p_Length){

        static int currIndex = -1;
        static constexpr bool END_OF_BUFFER = true;
        if (++currIndex == mBufferSize - 1)
            return END_OF_BUFFER;

        *p_Data = reinterpret_cast<char*>(std::addressof(mBuffer[currIndex]));
        p_Length = mBufferStatesPerSlot[currIndex].second;
        return !END_OF_BUFFER;
    }

private:
    constexpr std::size_t getPositionAfter(std::size_t pos) noexcept{

        // Implement required policy to roll buffer
        return ++pos == mBufferSize ? 0 : pos;
    }

    T* mBuffer;
    const std::size_t mBufferSize;
    std::atomic<std::size_t> mWritePos{0}, mReadPos{0};
    std::array<std::pair<std::atomic_bool, std::int32_t>, N> mBufferStatesPerSlot;

    // Helper states
    char* mCurrentWriteBuffer;
    std::size_t mBytesWrittenInCurrentWriteBuffer{0};
};

#endif /* FLOG_CIRCULAR_BUFFER_HPP */

