#include "cris/core/msg_recorder/rolling_helper.h"

#include "cris/core/utils/logging.h"
#include "cris/core/utils/time.h"

#include <algorithm>
#include <chrono>
#include <ratio>
#include <stdexcept>

namespace cris::core {

RollingHelper::RollingHelper(const RecordDirPathGenerator* const dir_path_generator)
    : dir_path_generator_{dir_path_generator} {
    if (dir_path_generator_ == nullptr) {
        throw std::logic_error{"Dir path generator must be set for rolling."};
    }
    if (!*dir_path_generator_) {
        throw std::logic_error{"Dir path generator must be callable for rolling."};
    }
}

void RollingHelper::Reset() {
}

std::filesystem::path RollingHelper::GenerateFullRecordDirPath() const {
    return (*dir_path_generator_)();
}

RollingByDayHelper::RollingByDayHelper(const RecordDirPathGenerator* const dir_path_generator)
    : RollingHelper{dir_path_generator}
    , last_write_time_{std::chrono::system_clock::now()} {
}

bool RollingByDayHelper::NeedToRoll(const Metadata& metadata) const {
    return metadata.time > last_write_time_ && !SameUtcDay(last_write_time_, metadata.time);
}

void RollingByDayHelper::Update(const Metadata& metadata) {
    last_write_time_ = std::max(metadata.time, last_write_time_);
}

RollingByHourHelper::RollingByHourHelper(const RecordDirPathGenerator* const dir_path_generator)
    : RollingByDayHelper{dir_path_generator} {
}

bool RollingByHourHelper::NeedToRoll(const Metadata& metadata) const {
    return metadata.time > last_write_time_ && !SameUtcHour(last_write_time_, metadata.time);
}

RollingBySizeHelper::RollingBySizeHelper(
    const RecordDirPathGenerator* const dir_path_generator,
    const std::uint64_t                 size_limit_mb)
    : RollingHelper{dir_path_generator}
    , limit_bytesize_{size_limit_mb * std::mega::num} {
}

bool RollingBySizeHelper::NeedToRoll(const Metadata& metadata) const {
    if (limit_bytesize_ <= metadata.value_size) {
        return true;
    }

    return current_bytesize_ >= limit_bytesize_ - metadata.value_size;
}

void RollingBySizeHelper::Update(const Metadata& metadata) {
    current_bytesize_ += metadata.value_size;
}

void RollingBySizeHelper::Reset() {
    current_bytesize_ = 0;
}

}  // namespace cris::core
