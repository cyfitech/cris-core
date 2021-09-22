#pragma once

#include "cris/core/message/queue.h"
#include "cris/core/node/base.h"

#include <condition_variable>
#include <mutex>

namespace cris::core {

class CRNode : public CRNodeBase {
   public:
    virtual ~CRNode();

    void Kick() override;

   private:
    void WaitForMessageImpl(std::chrono::nanoseconds timeout) override;

    void SubscribeImpl(std::string &&message_name, std::function<void(const CRMessageBasePtr &)> &&callback) override;

    std::vector<std::string> subscribed_{};
    std::mutex               wait_message_mutex_{};
    std::condition_variable  wait_message_cv_{};
};

}  // namespace cris::core
