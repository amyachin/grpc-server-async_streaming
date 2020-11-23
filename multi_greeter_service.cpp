#include "multi_greeter_service.h"
#include <sstream>
#include <memory>
#include <chrono>
#include <grpcpp/alarm.h>


using namespace std;
using hellostreamingworld::HelloReply;
using hellostreamingworld::HelloRequest;

class SayHelloStreamingHandler : public AsyncCallHandler<SayHelloStreamingHandler> {
public:
    SayHelloStreamingHandler(
        MultiGreeterService* service,
        grpc::ServerCompletionQueue* cq
        )
    :cq_(cq), 
    service_(service), 
    context_(), 
    state_(CREATED),
    writer_(&context_) {
            
    }

    virtual void Proceed() override {

        if (state_ == CREATED) {
            // Initial status
            state_ = PROCESS;
            // Register async handler for 
            service_->RequestsayHello(&context_, &request_, &writer_, cq_, cq_, Tag());
        } else if (state_ == PROCESS) {
            // Request has been recieved
            state_ = REPLYING;
            // Register another instance for new calls
            registry()->Register(new SayHelloStreamingHandler(service_, cq_));   
            currentReply_ = 1;

            ostringstream os;
            os << "Hello, " << request_.name() << " (" << currentReply_++ << ")";

            reply_.set_message(os.str());    
            if (request_.num_greetings() <= 1) {
                // Since it is the last reply We are done
                state_ = FINISHED;
                writer_.WriteAndFinish(reply_, grpc::WriteOptions(), grpc::Status::OK, Tag());    
            } else {
                writer_.Write(reply_, Tag());
            }
        } else if (state_ == REPLYING || state_ == TIMER_ELAPSED) {
            
            if (state_ == REPLYING && request_.pauseinmilliseconds() > 0) {
                state_ = TIMER_ELAPSED;
                alarm_.Set(cq_, std::chrono::system_clock::now() + 
                    std::chrono::milliseconds(request_.pauseinmilliseconds()), 
                    Tag());
                return;
            }
            state_ = REPLYING;
            ostringstream os;
            os << "Hello, " << request_.name() << " (" << currentReply_++ << ")";

            reply_.set_message(os.str());    
            if (request_.num_greetings() < currentReply_) {
                // We have reached the last reply
                state_ = FINISHED;
                writer_.WriteAndFinish(reply_, grpc::WriteOptions(), grpc::Status::OK, Tag());    
            } else {
                writer_.Write(reply_, Tag());
            }        
        } else  {
            GPR_ASSERT(state_ == FINISHED);
            Unregister();
        }

    }

    grpc::ServerCompletionQueue* cq_;
    MultiGreeterService* service_;
    HelloRequest request_;
    HelloReply reply_;
    grpc::ServerContext context_;
    grpc::ServerAsyncWriter<HelloReply> writer_;

    enum State {
        CREATED = 0,
        PROCESS = 1,
        REPLYING = 2,
        TIMER_ELAPSED = 3,
        FINISHED = 4
    };

    State state_;
    int currentReply_;
    grpc::Alarm alarm_;        

};


void MultiGreeterService:: BuildAsyncHandlers (
    HandlerRegistry* registry, grpc::ServerCompletionQueue* cq
) {
    registry->Register(new SayHelloStreamingHandler(this, cq));
}