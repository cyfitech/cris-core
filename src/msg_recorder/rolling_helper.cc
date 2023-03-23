#include "cris/core/msg_recorder/rolling_helper.h"

#include "cris/core/utils/logging.h"
#include "cris/core/utils/time.h"

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/core.h>

#include <algorithm>
#include <chrono>
#include <ratio>
#include <stdexcept>
#include <string>

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
    , last_write_time_{boost::posix_time::second_clock::universal_time()} {
}

bool RollingByDayHelper::NeedToRoll(const Metadata& metadata) const {
    return metadata.time > last_write_time_ && !SameDay(last_write_time_, metadata.time);
}

void RollingByDayHelper::Update(const Metadata& metadata) {
    last_write_time_ = std::max(metadata.time, last_write_time_);
}

RollingByHourHelper::RollingByHourHelper(const RecordDirPathGenerator* const dir_path_generator)
    : RollingByDayHelper{dir_path_generator} {
}

bool RollingByHourHelper::NeedToRoll(const Metadata& metadata) const {
    return metadata.time > last_write_time_ && !SameHour(last_write_time_, metadata.time);
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

std::string GetCurrentUtcDate() {
    return boost::gregorian::to_iso_string(boost::gregorian::day_clock::universal_day());
}

std::string GetCurrentUtcHour() {
    const auto utc_time = boost::posix_time::second_clock::universal_time();
    return fmt::format("{}{:02}", boost::gregorian::to_iso_string(utc_time.date()), utc_time.time_of_day().hours());
}

std::string GetCurrentUtcTime() {
    return boost::posix_time::to_iso_string(boost::posix_time::second_clock::universal_time());
}

bool SameDay(const boost::posix_time::ptime x, const boost::posix_time::ptime y) {
    return x.date().operator==(y.date());
}

bool SameHour(const boost::posix_time::ptime x, const boost::posix_time::ptime y) {
    return SameDay(x, y) && x.time_of_day().hours() == y.time_of_day().hours();
}

}  // namespace cris::core
