#include "multi_greeter_service.h"
#include "hellostreamingworld.grpc.pb.h"
#include "async_call_handler.h"
#include "multi_greeter_service.h"

#include <iostream>
#include <string>
#include <atomic>
#include <unordered_map>
#include <vector>


using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;

using hellostreamingworld::HelloRequest;
using hellostreamingworld::HelloReply;
using hellostreamingworld::MultiGreeter; 


class ServerImpl {
public:

    ~ServerImpl() {
        server_->Shutdown();
        // Always shutdown the completion queue after the server.
        cq_->Shutdown();
    }




    // There is no shutdown handling in this code.
    void Run() {
        std::string server_address("0.0.0.0:50051");

        ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        // Register "service_" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *asynchronous* service.
        builder.RegisterService(&service_);
        // Get hold of the completion queue used for the asynchronous communication
        // with the gRPC runtime.
        cq_ = builder.AddCompletionQueue();
        // Finally assemble the server.
        server_ = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;


        // Register async callback handlers
        // Proceed to the server's main loop.
        HandleRpcs();
    }


    void HandleRpcs() {

        HandlersRegistry registry;
        std::vector<std::unique_ptr<AsyncCallHandlerInterface>> handlers;
        service_.BuildAsyncHandlers(&registry, cq_.get());
        
        // Loop
        void* tag;  // uniquely identifies a request.
        bool ok;
        // Block waiting to read the next event from the completion queue. The
        // event is uniquely identified by its tag, which in this case is the
        // memory address of a CallData instance.
        // The return value of Next should always be checked. This return value
        // tells us whether there is any kind of event or cq_ is shutting down.
        while (cq_->Next(&tag, &ok)) {
            
            // Assuming that 
            int id = reinterpret_cast<intptr_t>(tag);

            if (!ok) {
                std::cout << "Tag " << id << " - completion queue did not succeed, unregistering the handler";
                registry.Unregister(id);
                continue; 
            }

            AsyncCallHandlerInterface* handler;
            
            if (registry.TryLookupById(id, &handler)) {
                handler->Proceed();
            } else {
                std::cout << "Unknown Tag: " << id;
            } 
        }
    }



private:
  std::unique_ptr<ServerCompletionQueue> cq_;
  MultiGreeterService service_;
  std::unique_ptr<Server> server_;
};





int main(int, char**) {

  ServerImpl server;
  server.Run();

  return 0;}
