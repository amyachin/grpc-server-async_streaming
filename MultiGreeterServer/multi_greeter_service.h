#ifndef SRC_MULTI_GREETER_SERVICE_H_
#define SRC_MULTI_GREETER_SERVICE_H_
#include <memory>
#include <vector>
#include <grpcpp/grpcpp.h>
#include "async_call_handler.h"
#include "hellostreamingworld.grpc.pb.h"

using hellostreamingworld::MultiGreeter;

class MultiGreeterService : public MultiGreeter::AsyncService {
public:
    void BuildAsyncHandlers(HandlersRegistry* registry, grpc::ServerCompletionQueue* cq);        
};


#endif /* SRC_MULTI_GREETER_SERVICE_H_ */
