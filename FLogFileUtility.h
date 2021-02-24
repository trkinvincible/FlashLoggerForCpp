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

        mFile = open(p_FileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
        mLogFile.reset(new FileOutputStream(mFile));
    }

    ~FileUtility(){

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
#ifdef DEBUG
                std::cout << "log file updated: " << mLogFile->ByteCount() << std::endl;
#endif
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
