#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "message/queue.h"

namespace cris::core {

// The nodes are supposed to live throughout the entire message flow. Do not destruct it when
// messages are coming
class CRNodeBase {
   public:
    CRNodeBase() = default;

    CRNodeBase(const CRNodeBase &) = delete;

    CRNodeBase(CRNodeBase &&) = delete;

    CRNodeBase &operator=(const CRNodeBase &) = delete;

    virtual ~CRNodeBase();

    template<class duration_t>
    void WaitForMessage(duration_t &&timeout);

    void Kick();

    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming, nor call it after Node Runner
    // is Running
    template<CRMessageType message_t, CRMessageCallbackType callback_t>
    void Subscribe(callback_t &&callback);

    void Publish(CRMessageBasePtr &&message);

    virtual CRMessageQueue *MessageQueueMapper(const CRMessageBasePtr &message) = 0;

    static CRNodeBase *GetMessageManager();

   private:
    virtual void SubscribeImpl(std::string &&                                  message_name,
                               std::function<void(const CRMessageBasePtr &)> &&callback) = 0;

    virtual std::vector<CRMessageQueue *> GetNodeQueues() = 0;

    friend class CRNodeRunner;

    std::vector<std::string> mSubscribed{};
    std::mutex               mWaitMessageMutex{};
    std::condition_variable  mWaitMessageCV{};
};

template<class duration_t>
void CRNodeBase::WaitForMessage(duration_t &&timeout) {
    std::unique_lock<std::mutex> lock(mWaitMessageMutex);
    mWaitMessageCV.wait_for(lock, timeout);
}

template<CRMessageType message_t, CRMessageCallbackType callback_t>
void CRNodeBase::Subscribe(callback_t &&callback) {
    auto message_name = CRMessageBase::GetMessageTypeName<message_t>();
    if (std::find(mSubscribed.begin(), mSubscribed.end(), message_name) != mSubscribed.end()) {
        // TODO WARNING: subscribed
        return;
    }
    mSubscribed.push_back(message_name);

    CRMessageBase::Subscribe(message_name, this);
    return SubscribeImpl(
        std::move(message_name), [callback = std::move(callback)](const CRMessageBasePtr &message) {
            return callback(reinterpret_cast<const std::shared_ptr<message_t> &>(message));
        });
}

}  // namespace cris::core
