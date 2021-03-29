#pragma once

#include <chrono>

namespace cris::core {

class CRNodeBase {
   public:
    template<class duration_t>
    void WaitForMessage(duration_t &&timeout);

    virtual void WaitForMessageImpl(const std::chrono::nanoseconds &timeout) = 0;

    virtual void Kick() = 0;

    virtual ~CRNodeBase() = default;
};

template<class duration_t>
void CRNodeBase::WaitForMessage(duration_t &&timeout) {
    return WaitForMessageImpl(std::chrono::duration_cast<std::chrono::nanoseconds>(timeout));
}

}  // namespace cris::core
