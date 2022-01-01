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

#pragma once

#include <iostream>
#include <cstdlib>
#include <atomic>
#include <deque>
#include <variant>
#include <memory>
#include <type_traits>

#include "FLogUtilStructs.h"

class ProducerMsg;

template<typename T>
class FLogCircularBuffer {

    static constexpr bool SLOT_LOCKED   = true;
    static constexpr bool SLOT_UNLOCKED = false;
    static constexpr int  CACHELINE_SIZE{64};

public:
    FLogCircularBuffer(const std::size_t p_BufferSize)
        :mBufferSize(p_BufferSize + 1),
         mBuffer(static_cast<T*>(std::aligned_alloc(CACHELINE_SIZE, sizeof(T) * p_BufferSize))),
         mCurrentWriteBuffer(reinterpret_cast<std::uint8_t*>(&mBuffer[mWritePos])){

        std::fill_n(reinterpret_cast<std::uint8_t*>(mBuffer), sizeof(T) * p_BufferSize, 0);
        mBufferStatesPerSlot.resize(p_BufferSize);
        static_assert(std::atomic<SlotsState>().is_always_lock_free, "SlotsState must be aggregate class");
    }

    FLogCircularBuffer(const FLogCircularBuffer&) = delete;
    FLogCircularBuffer& operator=(const FLogCircularBuffer&) = delete;
    FLogCircularBuffer(FLogCircularBuffer&&) = delete;
    FLogCircularBuffer& operator=(FLogCircularBuffer&&) = delete;

    ~FLogCircularBuffer(){

        std::free(mBuffer);
    }

    bool WriteData(const ProducerMsg& p_data){

        // Design note:
        // # - step1 check if slot is locked before writing to it
        // # - step2 if free lock it for writing
        // # - step3 once written unlock slot and move forward for next write

        auto pos = mWritePos;

        const SlotsState& ss = std::atomic_load_explicit(&mBufferStatesPerSlot[pos], std::memory_order_acquire);
        if (ss.state_is_locked == SLOT_LOCKED && ss.data_length != 0){
            return false;
        }

        // at log end current slot will be unlocked and mBytesWrittenInCurrentWriteBuffer will be reset.
        if (mBytesWrittenInCurrentWriteBuffer == 0){

            SlotsState new_ss{0, SLOT_LOCKED};
            std::atomic_store_explicit(&mBufferStatesPerSlot[pos], new_ss, std::memory_order_release);
            // mCurrentWriteBuffer is always linear next so fix it in previous write itself.
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
                    static_assert(always_false_v<ArgT>, "unsupported type");
                }
            }, v);
        }

        if (p_data.isEnd){

            // Reset all helpers
            auto len = std::distance(mCurrentWriteBuffer, mCurrentWriteBuffer + mBytesWrittenInCurrentWriteBuffer);
#if 1
            std::string test;
            test.resize(sizeof(T));
            memcpy(test.data(), &mBuffer[pos], len);
            std::cout << test;
#endif
            SlotsState new_ss{len, SLOT_UNLOCKED};
            std::atomic_store_explicit(&mBufferStatesPerSlot[pos], new_ss, std::memory_order_release);

            // prepare for next write
            mWritePos = getPositionAfter(pos);
            mBytesWrittenInCurrentWriteBuffer = 0;
            mCurrentWriteBuffer = reinterpret_cast<std::uint8_t*>(&mBuffer[mWritePos]);
        }

        return true;
    }

    bool ReadData(std::uint8_t** p_Data, std::size_t& p_Length, std::size_t& p_CurPos){

        // Design note:
        // # - step1 check if slot is locked before reading from it
        // # - step2 if free lock it for reading
        // # - step3 in UnlockReadPos() unlock slot after writing to file.

        auto pos = mReadPos;

        const SlotsState& ss = std::atomic_load_explicit(&mBufferStatesPerSlot[pos], std::memory_order_acquire);
        if (ss.state_is_locked == SLOT_LOCKED || ss.data_length == 0){
            return false;
        }

        SlotsState new_ss{ss.data_length, SLOT_LOCKED};
        std::atomic_store_explicit(&mBufferStatesPerSlot[pos], new_ss, std::memory_order_release);

        p_CurPos = pos;
        *p_Data = reinterpret_cast<std::uint8_t*>(std::addressof(mBuffer[pos]));
        p_Length = ss.data_length;
    }

    void UnlockReadPos(const std::size_t p_Pos)noexcept{

        SlotsState new_ss{0, SLOT_UNLOCKED};
        std::atomic_store_explicit(&mBufferStatesPerSlot[p_Pos], new_ss, std::memory_order_release);
        mReadPos = getPositionAfter(p_Pos);
    }

    bool FlushBuffer(std::uint8_t** p_Data, std::size_t& p_Length){

        static int currIndex = -1;
        static constexpr bool END_OF_BUFFER = true;
        if (++currIndex == mBufferSize - 1)
            return END_OF_BUFFER;

        *p_Data = reinterpret_cast<std::uint8_t*>(std::addressof(mBuffer[currIndex]));
        const SlotsState& ss = std::atomic_load(&mBufferStatesPerSlot[currIndex]);
        p_Length = ss.data_length;
        return !END_OF_BUFFER;
    }

private:
    constexpr std::size_t getPositionAfter(std::size_t pos) noexcept{

        // Implement required policy to roll buffer. STL style one past last element is end.
        return ++pos == mBufferSize ? 0 : pos;
    }

    std::size_t mWritePos{0};
    std::size_t mReadPos{0};

    const std::size_t mBufferSize;
    T* const mBuffer;

    // pack them together
    struct SlotsState{
        std::uint32_t data_length{0};
        bool state_is_locked{SLOT_UNLOCKED};
    };
    std::deque<std::atomic<SlotsState>> mBufferStatesPerSlot;

    // Helper states
    std::uint8_t* mCurrentWriteBuffer{nullptr};
    std::size_t mBytesWrittenInCurrentWriteBuffer{0};
};
