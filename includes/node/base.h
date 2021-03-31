#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "message/queue.h"

namespace cris::core {

class CRNodeBase {
   public:
    CRNodeBase(const CRNodeBase &) = delete;

    CRNodeBase(CRNodeBase &&) = delete;

    CRNodeBase &operator=(const CRNodeBase &) = delete;

    virtual ~CRNodeBase() = default;

    template<class duration_t>
    void WaitForMessage(duration_t &&timeout);

    void Kick();

    template<class message_t, class callback_t>
    void Subscribe(callback_t &&callback);

    virtual CRMessageQueue *MessageQueueMapper(const CRMessageBasePtr &message) = 0;

   private:
    virtual void SubscribeImpl(std::string &&                                  message_name,
                               std::function<void(const CRMessageBasePtr &)> &&callback) = 0;

    std::mutex              mWaitMessageMutex{};
    std::condition_variable mWaitMessageCV{};
};

template<class duration_t>
void CRNodeBase::WaitForMessage(duration_t &&timeout) {
    std::unique_lock<std::mutex> lock(mWaitMessageMutex);
    mWaitMessageCV.wait_for(timeout);
}

void CRNodeBase::Kick() {
    mWaitMessageCV.notify_all();
}

template<class message_t, class callback_t>
void CRNodeBase::Subscribe(callback_t &&callback) {
    static_assert(std::is_base_of_v<CRMessageBase, message_t>);
    static_assert(
        std::is_void_v<decltype(callback(std::declval<const std::shared_ptr<message_t> &>()))>);
    CRMessageBase::Subscribe<message_t>(this);
    return SubscribeImpl(
        CRMessageBase::GetMessageTypeName<message_t>(),
        [callback = std::move(callback)](const CRMessageBasePtr &message) {
            return callback(reinterpret_cast<const std::shared_ptr<message_t> &>(message));
        });
}

}  // namespace cris::core
