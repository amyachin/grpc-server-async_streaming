#ifndef CHATROOM_SERVICE_H_
#define CHATROOM_SERVICE_H_

#include <memory>
#include <grpcpp/grpcpp.h>
#include "async_call_handler.h"
#include "chatroom.grpc.pb.h"

using chatroom::ChatRoom;
using chatroom::RegistrationRequest;
using chatroom::RegistrationReply;
using chatroom::ListUsersRequest;
using chatroom::ListUsersResponse;
using chatroom::InboundMessage;

struct EventListenerInterface {
    
    virtual void PostMessage(std::shared_ptr<InboundMessage> msg) = 0;

};

class ChatRoomService : public  ChatRoom::AsyncService {

    // Bail out from handling chat method synchronously

    // virtual ::grpc::Status chat(::grpc::ServerContext* context, ::grpc::ServerReaderWriter< ::chatroom::InboundMessage, ::chatroom::OutboundMessage>* stream);
    //virtual ::grpc::Status registerUser(::grpc::ServerContext* context, const RegistrationRequest* request, RegistrationReply* response) override;
    //virtual ::grpc::Status listUsers(::grpc::ServerContext* context, const ListUsersRequest* request, ListUsersResponse* response) override;

public:

    ChatRoomService();

    void EnterRoom(const std::string& userName, int sessionId, EventListenerInterface* listener);
    
    void LeaveRoom(int sessionId);

    void BroadcastMessage(int sessionId, const std::string& message);

    void BuildAsyncHandlers(HandlerRegistry* registry, grpc::ServerCompletionQueue* cq);

    void ListAllUsers(std::vector<std::string> & list); 

private:
    class ChatRoomData;

    std::shared_ptr<ChatRoomData> pimpl_;
};


#endif /* CHATROOM_SERVICE_H_ */
