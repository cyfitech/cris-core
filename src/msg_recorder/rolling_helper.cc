#include "cris/core/msg_recorder/rolling_helper.h"

#include "cris/core/msg_recorder/impl/utils.h"
#include "cris/core/utils/logging.h"
#include "cris/core/utils/time.h"

#include <algorithm>
#include <chrono>
#include <ratio>
#include <string>

namespace cris::core {

using TimePoint = RollingHelper::Metadata::TimePoint;

std::string RollingHelper::MakeNewRecordDirName() const {
    return DefaultLevelDBDir();
}

RollingByDayHelper::RollingByDayHelper()
    : RollingByDayHelper{CalcNextRollingTime(std::chrono::system_clock::now(), kSecondsPerDay, 60)} {
}

RollingByDayHelper::RollingByDayHelper(const TimePoint time_to_roll) : time_to_roll_{time_to_roll} {
}

bool RollingByDayHelper::NeedToRoll(const Metadata& metadata) const {
    return metadata.time >= time_to_roll_;
}

void RollingByDayHelper::Update(const Metadata&) {
}

void RollingByDayHelper::Reset() {
    time_to_roll_ = CalcNextRollingTime(std::chrono::system_clock::now(), kSecondsPerDay, 60);
}

TimePoint RollingByDayHelper::CalcNextRollingTime(
    const TimePoint now,
    const int       interval_len,
    const int       offset_seconds) {
    const auto unix_time         = std::chrono::system_clock::to_time_t(now);
    const auto unix_time_to_roll = unix_time - unix_time % interval_len + interval_len + offset_seconds;
    return std::chrono::system_clock::from_time_t(unix_time_to_roll);
}

RollingByHourHelper::RollingByHourHelper()
    : RollingByDayHelper{CalcNextRollingTime(std::chrono::system_clock::now(), kSecondsPerHour, 60)} {
}

void RollingByHourHelper::Reset() {
    time_to_roll_ = CalcNextRollingTime(std::chrono::system_clock::now(), kSecondsPerHour, 60);
}

RollingBySizeHelper::RollingBySizeHelper(const std::uint64_t size_limit_mb)
    : limit_bytesize_{size_limit_mb * std::mega::num} {
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

std::string DefaultLevelDBDir() {
    std::string dirname = GetCurrentUtcTime();
    dirname.append(impl::kLevelDbDirSuffix);
    return dirname;
}

}  // namespace cris::core
