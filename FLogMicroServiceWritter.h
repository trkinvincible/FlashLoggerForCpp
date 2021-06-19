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
#include <memory>
#include <string>
#include <thread>

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include "flog.grpc.pb.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using FLogProto::FLogRemoteLogger;
using FLogProto::Response;
using FLogProto::LogLine;

class FLogMicroServiceWritter
{
public:
    explicit FLogMicroServiceWritter(const std::string& p_ServerColonPort){

        stub_ = FLogRemoteLogger::NewStub(grpc::CreateChannel(
                                         p_ServerColonPort, grpc::InsecureChannelCredentials()));
        std::thread(&FLogMicroServiceWritter::AsyncCompleteRpc, this).detach();
    }

    ~FLogMicroServiceWritter(){

        mResponseValidatorExit.store(true, std::memory_order_release);
    }

    void AsyncCompleteRpc() {

        void* got_tag;
        bool ok = false;
        while (cq_.Next(&got_tag, &ok)) {

            if (mResponseValidatorExit.load(std::memory_order_relaxed)) return;

            AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);
            GPR_ASSERT(ok);
            if (!call->status.ok())
                assert(0);
            delete call;
        }
    }

    bool WriteToFile(const char* data, int size) {

        LogLine request;
        std::string tmp(data, size);
        request.set_log(std::move(tmp));
        AsyncClientCall* call = new AsyncClientCall;
        call->response_reader = stub_->PrepareAsyncSendLogLine(&call->context, request, &cq_);
        call->response_reader->StartCall();
        call->response_reader->Finish(&call->reply, &call->status, (void*)call);
        return true;
    }

private:
    // struct for keeping state and data information
    struct AsyncClientCall {

      Response reply;
      ClientContext context;
      Status status;
      std::unique_ptr<ClientAsyncResponseReader<Response>> response_reader;
    };
    std::unique_ptr<FLogRemoteLogger::Stub> stub_;
    CompletionQueue cq_;
    std::atomic_bool mResponseValidatorExit;
};
