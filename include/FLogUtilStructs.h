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

#ifndef FLOG_UTIL_HPP
#define FLOG_UTIL_HPP

#include <variant>
#include <array>
#include <string>

using namespace std::chrono_literals;
const std::string s_copyright =
R"(
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



)";
enum class LEVEL : uint8_t{
  INFO = 0,
  WARN,
  CRIT
};

enum class GRANULARITY : uint8_t{
  BASIC,
  FULL
};

using supported_loggable_type = std::variant<const char*, unsigned int, int, double>;
struct ProducerMsg{

    ProducerMsg(bool p_IsEnd, std::array<supported_loggable_type, 8>&& p_Data):isEnd(p_IsEnd){ data.swap(p_Data); }
    bool isEnd = false;
    std::array<supported_loggable_type, 8> data;
};

inline uint64_t FLogNow(){

    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// helper constant for the visitor #3
template<class> inline constexpr bool always_false_v = false;

#define FLOG_INFO FLogLine() = FLogManager::globalInstance().getFlogLine(LEVEL::INFO, __FUNCTION__, __LINE__)
#define FLOG_WARN FLogLine() = FLogManager::globalInstance().getFlogLine(LEVEL::WARN, __FUNCTION__, __LINE__)
#define FLOG_CRIT FLogLine() = FLogManager::globalInstance().getFlogLine(LEVEL::CRIT, __FUNCTION__, __LINE__)

#endif /* FLOG_UTIL_HPP */

