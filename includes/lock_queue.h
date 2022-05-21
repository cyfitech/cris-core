#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace cris::core {

namespace impl {

class LockQueueBase {
   public:
    using Self = LockQueueBase;

    LockQueueBase(std::size_t capacity);

    LockQueueBase(const Self&) = delete;
    LockQueueBase(Self&&)      = delete;
    LockQueueBase& operator=(const Self&) = delete;
    LockQueueBase& operator=(Self&&) = delete;

    virtual ~LockQueueBase() = default;

    std::size_t Size();

    bool IsEmpty();

    bool IsFull();

   protected:
    void AddImpl(std::function<void(const std::size_t)>&& add_to_index);

    void PopImpl(bool only_latest, std::function<void(const std::size_t)>&& pop_from_index);

    const std::size_t capacity_;
    std::size_t       size_{0};
    std::size_t       begin_{0};
    std::size_t       end_{0};
    std::mutex        mutex_;
};

}  // namespace impl

template<class data_t>
class LockQueue : public impl::LockQueueBase {
   public:
    LockQueue(std::size_t capacity) : LockQueueBase(capacity) { buffer_.resize(capacity); }

    void Add(data_t&& message) {
        AddImpl([this, message = std::move(message)](const std::size_t idx) { buffer_[idx] = std::move(message); });
    }

    data_t Pop(bool only_latest) {
        data_t value{};
        PopImpl(only_latest, [this, &value](const std::size_t idx) { value = std::move(buffer_[idx]); });
        return value;
    }

   private:
    std::vector<data_t> buffer_;
};

}  // namespace cris::core
