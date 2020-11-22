
/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "hellostreamingworld.grpc.pb.h"

using grpc::Channel;
using grpc::ClientAsyncReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using hellostreamingworld::HelloRequest;
using hellostreamingworld::HelloReply;
using hellostreamingworld::MultiGreeter;

class MultiGreeterClient {
 public:
  MultiGreeterClient(std::shared_ptr<Channel> channel)
      : stub_(MultiGreeter::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  void SayHello(const std::string& user, int numberOfReplies, int pauseInMilliseconds) {
      // Data we are sending to the server.
      HelloRequest request;
      request.set_name(user);
      request.set_num_greetings(numberOfReplies);
      request.set_pauseinmilliseconds(pauseInMilliseconds);

  
    // Container for the data we expect from the server.
      HelloReply reply;
      // Context for the client. It could be used to convey extra information to
      // the server and/or tweak certain RPC behaviors.
      ClientContext context;

      // The producer-consumer queue we use to communicate asynchronously with the
      // gRPC runtime.
      CompletionQueue cq;

      // Storage for the status of the RPC upon completion.
      Status status;

      // stub_->PrepareAsyncSayHello() creates an RPC object, returning
      // an instance to store in "call" but does not actually start the RPC
      // Because we are using the asynchronous API, we need to hold on to
      // the "call" instance in order to get updates on the ongoing RPC.
      std::unique_ptr<ClientAsyncReader<HelloReply> > rpc(
          stub_->PrepareAsyncsayHello(&context, request, &cq));

      // StartCall initiates the RPC call
      rpc->StartCall((void*)1);

      void* got_tag;
      bool ok = false;

      // Block until the next result is available in the completion queue "cq".
      // The return value of Next should always be checked. This return value
      // tells us whether there is any kind of event or the cq_ is shutting down.
      while(cq.Next(&got_tag, &ok)) {

          int id = static_cast<int>((intptr_t)got_tag);
          // Verify that the result from "cq" corresponds, by its tag, our previous
          // request.
          GPR_ASSERT(id == 1 || id == 2);

          if (ok == false) {
              std::cout << "Server completed the stream" << std::endl;
              break;
          }

          rpc->Read(&reply, (void*)2);
          if (id == 2) {
              std::cout << id <<" Greeter received: " << reply.message() << std::endl;
          }
      }

      rpc->Finish(&status, (void*)3);
      if (cq.Next(&got_tag, &ok)){
          GPR_ASSERT(got_tag == (void*)3);
          GPR_ASSERT(ok);
          if (status.ok()) {
            std::cout << "RPC call succeeded" << std::endl;
          }
          else {
             std::cout << "RPC failed : " << status.error_message() << std::endl; 
          }
      }
  }

 

 private:
    
    // Out of the passed in Channel comes the stub, stored here, our view of the
    // server's exposed services.
    std::unique_ptr<MultiGreeter::Stub> stub_;

  
    // The producer-consumer queue we use to communicate asynchronously with the
    // gRPC runtime.
    CompletionQueue cq_;
};

int main(int argc, char** argv) {

  MultiGreeterClient greeter(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
  std::string user("world");
  
  std::cout << "Number of replies: "; 
  int numberOfRetries;
  std::cin >> numberOfRetries;
  std::cout << std::endl;
  
  int pauseInMilliseconds;
  std::cout << "Pause between greetings (milliseconds): "; 
  std::cin >> pauseInMilliseconds;
  greeter.SayHello(user,  numberOfRetries, pauseInMilliseconds);

  return 0;
}
