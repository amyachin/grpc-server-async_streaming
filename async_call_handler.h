#ifndef SRC_ASYNC_CALL_HANDLER_H_
#define SRC_ASYNC_CALL_HANDLER_H_

#include <memory>
#include <type_traits>
#include <unordered_map>

struct AsyncCallHandlerRegistry;

struct AsyncCallHandlerInterface {
    virtual ~AsyncCallHandlerInterface() {}
    virtual void Proceed() = 0;
    virtual void SetRegistry(AsyncCallHandlerRegistry * registry, int id) = 0; 
};

struct AsyncCallHandlerRegistry {
    virtual std::pair<int, AsyncCallHandlerInterface *>  Register(AsyncCallHandlerInterface * item) = 0;
    virtual void Unregister(int registerId) = 0;
};

namespace details {
    template<typename T>
    T* CloneObject(T* source, std::true_type tag) {
        return new T(*source);
    }

    template < typename T>
    T* CloneObject(T* source, std::false_type tag) {
        return static_cast<T*>(source->Clone());
    }
}

template < typename T >
std::unique_ptr<AsyncCallHandlerInterface> CloneCallHandler(T* source) {
    T* p = details::CloneObject(source, typename std::is_copy_constructible<T>::type());
    return std::unique_ptr<AsyncCallHandlerInterface>(static_cast<AsyncCallHandlerInterface*>(p));    
}

template < typename T >
class AsyncCallHandler : public AsyncCallHandlerInterface  {
public:

    AsyncCallHandler() 
        : id_(-1), registry_(nullptr) {
    }

    // Copy as unregistered by default
    AsyncCallHandler(const AsyncCallHandler& source) 
        : id_(-1), registry_(nullptr) {
    }
 
    // Creates a new registered instance of this handler type
    // Registration is not thread safe and is meant to be called from CQ thread
    //   
    std::pair<int, T*> RegisterCopy() {

        auto p = CloneCallHandler(static_cast<T*>(this));
        auto pair = registry_->Register(p.release());
        return std::make_pair(pair.first, static_cast<T*>(pair.second)); 
    }

    // Unsubscribes and invalidates the instance of this handler
    // Must be the last method to call
    void Unregister()  {
        if (registry_) {
            registry_->Unregister(id_); 
        }
    }


    void * Tag() const{
        return reinterpret_cast<void*>(id_);
    }

    int Id() const  {
        return id_;
    }

    // not copy assignable, but can use copy constructor
    AsyncCallHandler & operator = (const AsyncCallHandler & source) = delete; 

protected:

    AsyncCallHandlerRegistry * registry() const {
        return registry_;
    }
    
private:
    virtual void SetRegistry(AsyncCallHandlerRegistry* registry, int id) override  {
        registry_ = registry;
        id_ = id;
    } 

    int id_;
    AsyncCallHandlerRegistry * registry_;
};


class HandlerRegistry : public AsyncCallHandlerRegistry {

public:

    HandlerRegistry()
        : handlers_(), nextId_(0) {}

    
    virtual std::pair<int, AsyncCallHandlerInterface*> Register(AsyncCallHandlerInterface* item) override{
        int id = nextId_++;
        auto it = handlers_.emplace(id, item);
        it.first->second->SetRegistry(this,  id);
        it.first->second->Proceed(); // Initializes the handler
        return std::make_pair(id, it.first->second.get()); 
    }

    virtual void Unregister(int registerId) override {
        handlers_.erase(registerId);
    }
    
    bool TryLookupById(int id, AsyncCallHandlerInterface** out) {
        auto it = handlers_.find(id);
        if (it == handlers_.end()) {
            return false;
        }
        *out = it->second.get();
        return true;
    }

private:
    std::unordered_map<int, std::unique_ptr<AsyncCallHandlerInterface>> handlers_;
    int nextId_;
};


#endif /* SRC_ASYNC_CALL_HANDLER_H_ */
