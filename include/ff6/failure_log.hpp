#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace ff6 {
class FieldSession;
// Host diagnostics are separate from game saves and are disabled by default.
// The host catches I/O failures: logging must never interrupt gameplay.
class FailureLog {
  public:
    explicit FailureLog(std::filesystem::path folder);
    bool enabled() const { return enabled_; }
    void set_enabled(bool enabled);
    void record(std::string_view report);
    static constexpr unsigned retained_reports = 8;
    static constexpr std::size_t maximum_report_bytes = 64U * 1024U;
  private:
    std::filesystem::path folder_;
    bool enabled_ = false;
};
// Semantic state only: no screenshots, ROM data, dialogue or save snapshots.
std::string describe_session_failure(const FieldSession&, std::string_view reason);
} // namespace ff6
