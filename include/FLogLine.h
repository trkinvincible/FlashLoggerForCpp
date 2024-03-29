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

#ifndef FLOG_LINE_HPP
#define FLOG_LINE_HPP

#include <iostream>
#include <type_traits>
#include <variant>
#include <vector>
#include <chrono>
#include <iomanip>
#include <string.h>

#include "FLogUtilStructs.h"

void AddProdMsgExternal(ProducerMsg&&);

static constexpr int MAX_DATE_TIME_STRING_LENGTH =  20;
static constexpr int MAX_FUNCTION_NAME_LENGTH    =  70;
static constexpr int MAX_LINE_NAME_LENGTH        =   5;    // 5 digits
static constexpr int MAX_USER_DATA_LENGTH        = 159;
static constexpr int MAX_DELIMITER_LENGTH        =   1;
//                                                 ----
//                                                  256 (multiples of 64 so 4 cache lines)
//                                                 ----

static constexpr std::size_t MAX_LOG_LINE_SIZE{MAX_DATE_TIME_STRING_LENGTH + MAX_FUNCTION_NAME_LENGTH + MAX_LINE_NAME_LENGTH + MAX_USER_DATA_LENGTH + MAX_DELIMITER_LENGTH};
class alignas(MAX_LOG_LINE_SIZE + 1) FLogLine {

public:
    FLogLine() = default;

    void InitData(uint64_t p_Now, char const* p_Function, uint32_t p_Line){

        AddProdMsgExternal(ProducerMsg(false, {"[ ", Gettime(p_Now), " ]",
                                               "[ ", p_Function, " : ", p_Line, " ]"}));
    }

    const FLogLine& operator=(const FLogLine& other){ mIgnore = false; return *this; }

    ~FLogLine(){ if (!mIgnore) AddProdMsgExternal(ProducerMsg(true, {" \n"})); }

    virtual const FLogLine& operator<<(const supported_loggable_type&& p_Arg) const{

        std::visit([](auto&& arg) {

            using argT = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<argT, const char*> ||
                          std::is_same_v<argT, unsigned int>||
                          std::is_same_v<argT, int>         ||
                          std::is_same_v<argT, double>){

                        AddProdMsgExternal(ProducerMsg(false, {" ", arg}));
            }else{

                static_assert(always_false_v<argT>, "implement \"<<\" operator for un-supported type");
            }
        }, p_Arg);

        return *this;
    }

private:
    char* Gettime(uint64_t p_Now){

        // Need a fix here for corrupted date time. need a dynamic buffer.
        static char mbstr_CB[5][100];
        static std::int8_t index = -1;
        index = (++index % 5);
        char* mbstr = *(mbstr_CB + index);
        std::fill_n(mbstr, 100, 0);

        auto duration = std::chrono::microseconds(p_Now);
        std::chrono::system_clock::time_point time_point(duration);
        std::time_t time_t = std::chrono::system_clock::to_time_t(time_point);
        if (int len; len = std::strftime(mbstr, 100, "%c", std::localtime(&time_t))){

            static char microseconds[20];
            sprintf(microseconds, " micro-seconds: %03lu", duration.count());
            strncat(mbstr, microseconds, 19);
            return mbstr;
        }
        return "Empty";
    }

    mutable bool mIgnore{true};
};

// Null Object Design pattern
struct FLogLineDummy final : public FLogLine{

    const FLogLineDummy& operator<<(const supported_loggable_type&& p_Arg) const override{

        std::cout << "ignored: " << std::endl;
        return *this;
    }
};
#endif /* FLOG_LINE_HPP */

