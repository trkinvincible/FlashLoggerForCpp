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

#ifndef FILE_UTILITY_H
#define FILE_UTILITY_H

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using namespace google::protobuf::io;

class FileUtility
{
public:
    FileUtility(const std::string& p_FileName){

#if 0   // Release
        mFile = open(p_FileName.c_str(), S_IRUSR|S_IWUSR, O_WRONLY | O_CREAT | O_TRUNC);
#else
        mFile = open(p_FileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
#endif
        mLogFile.reset(new FileOutputStream(mFile));
    }

    ~FileUtility() noexcept{

        mLogFile.reset();
        close(mFile);
    }

    bool WriteToFile(const char* data, int size) {

        const uint8_t* in = reinterpret_cast<const uint8_t*>(data);
        int in_size = size;

        void* out;
        int out_size;

        while (true) {
            if (!mLogFile->Next(&out, &out_size)) {
                return false;
            }
            if (in_size <= out_size) {
                memcpy(out, in, in_size);
                mLogFile->BackUp(out_size - in_size);
                return true;
            }

            memcpy(out, in, out_size);
            in += out_size;
            in_size -= out_size;
        }
    }

private:
    int mFile;
    std::unique_ptr<ZeroCopyOutputStream> mLogFile;
};
#endif // FILE_UTILITY_H
