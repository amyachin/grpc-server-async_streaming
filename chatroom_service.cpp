#include "chatroom_service.h"
#include "async_call_handler.h"
#include <vector>
#include <queue>
#include <sstream>
#include <unordered_map>

using namespace chatroom;


class ChatWriteHandler;
class ChatMessageHandler;


// State shared by reader and writer
class ChatSession : public EventListenerInterface {

public:

    ChatSession(ChatRoomService * service, ::grpc::ServerCompletionQueue* cq )
        :service(service), cq(cq), 
        context(), 
        readerWriter(new grpc::ServerAsyncReaderWriter<InboundMessage, OutboundMessage>(&context)) {
    }

    void RequestChat( void* tag ) {

        context.AsyncNotifyWhenDone(tag); 
        service->Requestchat(&context, readerWriter.get(), cq, cq, tag);
    }

    void Unregister();

    void Init(int sessionId);

    virtual void PostMessage(std::shared_ptr<InboundMessage> msg) override; 

    void LeaveRoom() {

        userInChat_ = false;
        service->LeaveRoom(sessionId_);
    }

    void SetUserName(const std::string& userName ) {

        if (userInChat_) {
            LeaveRoom();
        }


        if (!userName.empty()) {
            userName_  = userName;
            service->EnterRoom(userName, sessionId_, this);
            userInChat_ = true;
        }
        else {
            userName_ = "???";
        }
    }

     void BroadcastMessage(const std::string& message) {
         
         if (userInChat_){
            service->BroadcastMessage(sessionId_, message);
         }
     }

     bool TrySayGoodBye();

    const std::string& UserName() const {
        return userName_;
    }

    int sessionId_;
    bool userInChat_;
    std::string userName_;
    ChatWriteHandler* writeHandler;
    ChatMessageHandler* messageHandler;
    ChatRoomService* service;
    ::grpc::ServerCompletionQueue* cq;
    grpc::ServerContext context;
    std::unique_ptr<grpc::ServerAsyncReaderWriter<InboundMessage, OutboundMessage>> readerWriter;
};


class ChatWriteHandler : public AsyncCallHandler<ChatWriteHandler> {
 public:

     ChatWriteHandler(std::shared_ptr<ChatSession> session)
     : session_(move(session)), state_(CREATED){
    }

    ~ChatWriteHandler() {
        session_->writeHandler = nullptr;
    }

    virtual void Proceed() override {
        
        if (state_ == CREATED) {
            // go to idle state
           state_ = IDLE;
           session_->writeHandler = this;                 
        } else if (state_ == IDLE) {
            // NOTHING TO DO
        } else if (state_ == WRITING) {
            state_ = IDLE;
            // Writing completed
            if (!queue_.empty()) {
                WriteMessage(*queue_.front().get(), queue_.size() == 1 && goodby_);
                queue_.pop();
            }
        }
        else {
            GPR_ASSERT(state_ == FINISHED);
            session_->Unregister();
        }
    }

    void SayWelcome() {
        response_.Clear();
        response_.mutable_message()->set_message("Welcome to the chat!");
        WriteMessage(response_, false);
    }

    void SayGoodbye() {

        goodby_ = true;

        if (state_ == WRITING || state_ == IDLE) {
            std::ostringstream s;
            s << "Good bye, " << session_->UserName() << ".";
            response_.Clear();
            response_.mutable_message()->set_message(s.str());

            if (state_ == IDLE) {
                WriteMessage(response_, true);
            }
            else {
                queue_.emplace(std::make_shared<InboundMessage>(response_));
            } 

        }
    } 

    void PostMessage(std::shared_ptr<InboundMessage> msg) {
        
        if (goodby_) {
            return; // refuse to send messages after goodby
        }

        if (state_ == IDLE) {
            WriteMessage(*msg.get(), false);
        }
    }


private:

    enum State  {
        CREATED = 0,
        WRITING = 1,
        IDLE = 2,
        FINISHED = 3
    };


    void WriteMessage(const InboundMessage & msg, bool last)  {
        GPR_ASSERT(state_ == IDLE);

        if (last) {
            state_ = FINISHED;
            session_->readerWriter->WriteAndFinish(msg, grpc::WriteOptions(), grpc::Status::OK, Tag());
        } else {
            state_ = WRITING;
            session_->readerWriter->Write(msg, Tag());
        }
    }

    std::queue<std::shared_ptr<InboundMessage>> queue_;

    bool goodby_;
    State state_;
    InboundMessage response_;
    std::shared_ptr<ChatSession> session_;
};


class ChatMessageHandler : public AsyncCallHandler<ChatMessageHandler> {
public:
    ChatMessageHandler(ChatRoomService * service, ::grpc::ServerCompletionQueue* cq ) 
    : session_(new ChatSession(service, cq)), state_(CREATED){
        session_->messageHandler = this;
    }

    ~ChatMessageHandler() {
        session_->messageHandler = nullptr;
    }

    virtual void Proceed() override {
        
        if (state_ != CREATED && session_->context.IsCancelled()) {
            // Stream or  connection is closed by the client
            session_->Unregister();
            return;
        }

        if (state_ == CREATED) {
            state_ = PROCESSING;
            session_->RequestChat(Tag());

            // instantiate and register writer for the same call
            registry()->Register(new ChatWriteHandler(session_));

        } else if (state_ == PROCESSING) {
            state_= CHATTING;

            // New call handler
            registry()->Register(new ChatMessageHandler(session_->service, session_-> cq));
            // Continue listening for the events   
            session_->readerWriter->Read(&request_, Tag());

            session_->Init(Id());
         
        } else if (state_ == CHATTING) {
            
            // message read completed
            switch(request_.test_one_of_case()) {
                case OutboundMessage::TestOneOfCase::kEvent:

                    if (request_.event().username().size() > 0) {
                        //Registration event
                        session_->SetUserName(request_.event().username());
                    }
                    else {

                         state_ = FINISHED;

                         if (session_->TrySayGoodBye()) {
                            state_ = GOODBYE;
                            return;    
                         }

                         session_->context.TryCancel();
                    }           
                    break;

                case OutboundMessage::TestOneOfCase::kMessage:
                    session_->BroadcastMessage(request_.message().message());
                    break;

                default:
                    break;
            };

        } else {
            GPR_ASSERT(state_ == FINISHED);
            session_->Unregister();
        } 

    } 

private:


    enum State  {
        CREATED = 0,
        PROCESSING = 1,
        CHATTING = 2,
        GOODBYE = 3,
        FINISHED = 4
    };


    OutboundMessage request_;
    State state_;
    grpc::ServerContext context_;
    std::shared_ptr<ChatSession> session_;
};



void ChatSession::Init(int sessionId) {
    sessionId_ = sessionId;
    if (writeHandler != nullptr) {
        writeHandler->SayWelcome();
    }

}

void ChatSession::Unregister() {
        
    if (userInChat_) {
        LeaveRoom();
    }

    if (writeHandler) {
        writeHandler->Unregister();
    }

    if (messageHandler) {
        messageHandler->Unregister();
    }

}

void ChatSession::PostMessage(std::shared_ptr<InboundMessage> msg) {
    if (writeHandler != nullptr) {
        writeHandler->PostMessage(msg);
    }
}

bool ChatSession::TrySayGoodBye() {
    
    if (writeHandler) {
        writeHandler->SayGoodbye();
        return true;
    }
    else {
        return false;
    }
}

class ListUsersHandler: public AsyncCallHandler<ListUsersHandler>{
public:
    ListUsersHandler(ChatRoomService * service, ::grpc::ServerCompletionQueue* cq )
    : service_(service), cq_(cq), state_(CREATED), context_(), 
    writer_(new grpc::ServerAsyncResponseWriter<ListUsersResponse>(&context_)) {

    }

    void Proceed() {

        if (state_ == CREATED) {
            state_ = PROCESSING;
            service_->RequestlistUsers(&context_, &request_, writer_.get(), cq_, cq_, Tag());
        }else if (state_ == PROCESSING) {

            state_ = FINISHED;
            registry()->Register(new ListUsersHandler(service_, cq_));
            std::vector<std::string> list;
            service_->ListAllUsers(list);

            for(auto &it: list) {
                response_.mutable_usernames()->Add(std::move(it));
            }

            writer_->Finish(response_, grpc::Status::OK, Tag());
        } else {
            GPR_ASSERT(state_ == FINISHED);
            Unregister();
        }
    }


    enum State {
        CREATED = 0,
        PROCESSING = 1,
        FINISHED = 2
    };

private:
    ::grpc::ServerContext context_;
    ListUsersResponse response_;
    ListUsersRequest request_;
    ChatRoomService* service_;
    ::grpc::ServerCompletionQueue* cq_;
    State state_;
    std::unique_ptr<grpc::ServerAsyncResponseWriter<ListUsersResponse>> writer_;
};



void ChatRoomService::BuildAsyncHandlers(HandlerRegistry* registry, grpc::ServerCompletionQueue* cq) {
    registry->Register(new ChatMessageHandler(this, cq));
    registry->Register(new ListUsersHandler(this, cq));
}


class ChatRoomService::ChatRoomData {
public:

    struct SessionInfo {
        SessionInfo(const std::string& userName, 
         EventListenerInterface* listener)
        : userName(userName),  listener(listener) {

        }

        EventListenerInterface* listener;
        std::string userName;
    };



    void EnterRoom(const std::string& userName, int sessionId, EventListenerInterface* listener) {
        sessions_.emplace(std::make_pair(sessionId,  SessionInfo(userName, listener)));
    }

    void LeaveRoom(int sessionId) {
        sessions_.erase(sessionId);
    }

    void BroadcastMessage(int sessionId, const std::string& message) {
        
        auto  msg = std::make_shared<InboundMessage>();
        for(auto& t : sessions_){
            if (t.first != sessionId && t.second.listener != nullptr) {
                t.second.listener->PostMessage(msg);
            }
        }
    }

    void ListAllUsers(std::vector<std::string> & list) {

        list.clear();
        for(auto &t : sessions_){
            list.push_back(t.second.userName);
        }
    }


private:

    std::unordered_map<int, SessionInfo> sessions_;

};


ChatRoomService::ChatRoomService(): pimpl_(new ChatRoomService::ChatRoomData()) {

}


void ChatRoomService::EnterRoom(const std::string& userName, int sessionId, EventListenerInterface* listener) {
    pimpl_->EnterRoom(userName, sessionId, listener);
}
    
void ChatRoomService::LeaveRoom(int sessionId) {
    pimpl_->LeaveRoom(sessionId);
}

void ChatRoomService::ListAllUsers(std::vector<std::string> & list) {
    pimpl_->ListAllUsers(list);
} 

void ChatRoomService::BroadcastMessage(int sessionId, const std::string& message) {
    pimpl_->BroadcastMessage(sessionId, message);
}

