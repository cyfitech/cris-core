#include <thread>

#include "cris/core/node/single_queue_node.h"

namespace cris::core {

class CRMessageManager : public CRSingleQueueNode {
   public:
    CRMessageManager(size_t queue_capacity)
        : CRSingleQueueNode(queue_capacity, &CRMessageBase::Dispatch) {}

   private:
    void SubscribeHandler(std::string &&                                  message_name,
                          std::function<void(const CRMessageBasePtr &)> &&callback) override;
};

CRNodeBase *CRNodeBase::GetMessageManager() {
    // TODO: make them configurable
    static const size_t kMessageManagerQueueCapacity = 8192;

    static CRMessageManager manager(kMessageManagerQueueCapacity);
    return &manager;
}

void CRMessageManager::SubscribeHandler(std::string &&message_name,
                                        std::function<void(const CRMessageBasePtr &)> &&callback) {
    // Message Manager never subscribe to any message
}

}  // namespace cris::core
