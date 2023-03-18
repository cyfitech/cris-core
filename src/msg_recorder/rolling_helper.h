#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <string_view>

namespace cris::core {

class RollingHelper {
   public:
    using RecordDirPathGenerator = std::function<std::filesystem::path()>;

    struct Metadata {
        using TimePoint = std::chrono::system_clock::time_point;

        TimePoint     time;
        std::uint64_t value_size{};
    };

    explicit RollingHelper(const RecordDirPathGenerator* const dir_path_generator);
    virtual ~RollingHelper() = default;

    virtual bool NeedToRoll(const Metadata metadata) const = 0;

    virtual void Update(const Metadata metadata) = 0;

    virtual void Reset();

    virtual std::filesystem::path GenerateFullRecordDirPath() const;

   private:
    const RecordDirPathGenerator* dir_path_generator_;
};

class RollingByDayHelper : public RollingHelper {
   public:
    explicit RollingByDayHelper(const RecordDirPathGenerator* const dir_path_generator);
    ~RollingByDayHelper() override = default;

    bool NeedToRoll(const Metadata metadata) const override;

    void Update(const Metadata metadata) override;

   protected:
    Metadata::TimePoint last_write_time_{};
};

class RollingByHourHelper : public RollingByDayHelper {
   public:
    explicit RollingByHourHelper(const RecordDirPathGenerator* const dir_path_generator);
    ~RollingByHourHelper() override = default;

    bool NeedToRoll(const Metadata metadata) const override;
};

class RollingBySizeHelper : public RollingHelper {
   public:
    explicit RollingBySizeHelper(
        const RecordDirPathGenerator* const dir_path_generator,
        const std::uint64_t                 size_limit_mb);
    ~RollingBySizeHelper() override = default;

    bool NeedToRoll(const Metadata metadata) const override;

    void Update(const Metadata metadata) override;

    void Reset() override;

   protected:
    const std::uint64_t limit_bytesize_;
    std::uint64_t       current_bytesize_{0u};
};

}  // namespace cris::core
