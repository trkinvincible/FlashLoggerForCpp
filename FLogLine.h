#ifndef FLOG_LINE_HPP
#define FLOG_LINE_HPP

#include <iostream>
#include <type_traits>
#include <variant>
#include <vector>
#include <chrono>
#include <iomanip>

#include "FLogUtilStructs.h"

void AddProdMsgExternal(ProducerMsg&&);

class FLogLine {

    static constexpr int MAX_DATE_TIME_STRING_LENGTH = 20;
    static constexpr int MAX_FUNCTION_NAME_LENGTH    = 100;
    static constexpr int MAX_LINE_NAME_LENGTH        = sizeof(uint32_t);
    static constexpr int MAX_USER_DATA_LENGTH        = 200;
    static constexpr int MAX_DELIMITER_LENGTH        = 1;

public:
    struct type {
       alignas(std::alignment_of<char>) unsigned char dummy[MAX_DATE_TIME_STRING_LENGTH +
       MAX_FUNCTION_NAME_LENGTH + MAX_LINE_NAME_LENGTH + MAX_USER_DATA_LENGTH + MAX_DELIMITER_LENGTH];
    };

    FLogLine() = default;
    void InitData(uint64_t p_Now, char const* p_Function, uint32_t p_Line){

        AddProdMsgExternal(ProducerMsg(false, {"[ ", Gettime(p_Now), " ]",
                                               "[ ", p_Function, " : ", p_Line, " ]"}));
    }

    const FLogLine& operator=(const FLogLine& other){ return *this; }

    ~FLogLine(){ AddProdMsgExternal(ProducerMsg(true, {" \n"})); }

    virtual const FLogLine& operator<<(const var_t&& p_Arg) const{

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
    const char* Gettime(uint64_t p_Now){

        static char mbstr[100];
        auto duration = std::chrono::seconds(p_Now);
        std::chrono::system_clock::time_point time_point(duration);
        std::time_t time_t = std::chrono::system_clock::to_time_t(time_point);
        if (std::strftime(mbstr, sizeof(mbstr), "%c", std::localtime(&time_t)))
            return mbstr;
        return "Empty";
    }

    // date & time %a and %T
    // pretty function
    // line number
    // userdata
};

#endif /* FLOG_LINE_HPP */

