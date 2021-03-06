#ifndef SRC_MULTI_GREETER_SERVICE_H_
#define SRC_MULTI_GREETER_SERVICE_H_

#include <grpcpp/grpcpp.h>
#include "async_call_handler.h"
#include "hellostreamingworld.grpc.pb.h"

using hellostreamingworld::MultiGreeter;

class MultiGreeterService : public MultiGreeter::AsyncService {
public:
    void BuildAsyncHandlers(HandlerRegistry* registry, grpc::ServerCompletionQueue* cq);
};


#endif /* SRC_MULTI_GREETER_SERVICE_H_ */
