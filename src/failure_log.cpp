#include "ff6/failure_log.hpp"
#include "ff6/field_session.hpp"
#include "ff6/io.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>

namespace ff6 {
namespace {
void publish(const std::filesystem::path& path, std::string_view text) {
    static std::atomic<unsigned> sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto temporary = path;
    temporary += "." + std::to_string(stamp) + "." + std::to_string(sequence++) + ".tmp";
    try {
        std::ofstream out(temporary, std::ios::binary);
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.close();
        std::filesystem::rename(temporary, path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
} // namespace
FailureLog::FailureLog(std::filesystem::path folder) : folder_(std::move(folder)) {
    // Absent, unreadable and malformed preferences all mean off. A normal
    // launch does not create diagnostic files or collect report data.
    try {
        const auto bytes = read_file(folder_ / "logging.cfg", 64);
        enabled_ = std::string(bytes.begin(), bytes.end()) == "FFVI failure logging 1\non\n";
    } catch (const std::exception&) {}
}
void FailureLog::set_enabled(bool enabled) {
    std::filesystem::create_directories(folder_);
    publish(folder_ / "logging.cfg", enabled ? "FFVI failure logging 1\non\n"
                                           : "FFVI failure logging 1\noff\n");
    enabled_ = enabled;
}
void FailureLog::record(std::string_view report) {
    if (!enabled_) return;
    std::filesystem::create_directories(folder_);
    const auto file = [&](unsigned i) { return folder_ / ("failure-" + std::to_string(i) + ".log"); };
    for (unsigned i = retained_reports - 1; i; --i)
        if (std::filesystem::exists(file(i - 1)))
            std::filesystem::rename(file(i - 1), file(i));
    constexpr std::string_view truncated = "\n[report truncated]\n";
    std::string bounded(report.substr(0, maximum_report_bytes - truncated.size()));
    if (bounded.size() < report.size()) bounded += truncated;
    publish(file(0), bounded);
}
std::string describe_session_failure(const FieldSession& session, std::string_view reason) {
    std::ostringstream out;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    out << "FFVI native failure report 1\nengine=" << FF6_ENGINE_VERSION << "\nunix_seconds=" << seconds
        << "\nreason=" << reason.substr(0, 8192) << "\ntick=" << session.ticks();
    const auto& seed = session.scene().state();
    out << " map=" << seed.state.field.current_map
        << " event_offset=" << seed.events.position()
        << " continuation=" << static_cast<unsigned>(seed.events.continuation())
        << " boundary=" << static_cast<unsigned>(session.boundary()) << '\n';
    if (const auto leader = seed.state.field.update_order[0]) {
        const auto& actor = seed.state.field.objects[*leader];
        out << "leader=" << unsigned(*leader) << " x=" << actor.x << " y=" << actor.y
            << " movement=" << unsigned(actor.movement.mode) << '\n';
    }
    if (session.world())
        out << "world=" << unsigned(session.world()->map) << " x=" << session.world()->x
            << " y=" << session.world()->y << '\n';
    if (session.battle_formation())
        out << "formation=" << session.battle_formation()->selection.formation << '\n';
    if (session.battle_menus())
        out << "battle_menu=" << static_cast<unsigned>(session.battle_menus()->menus().phase())
            << " completed_actions=" << session.battle_menus()->completed_actions() << '\n';
    if (const auto& battle = session.battle_state()) {
        out << "rng=" << unsigned(battle->random.index)
            << " counter_action=" << battle->retaliation_action
            << " immediate=" << battle->immediate_actions.size() << '\n';
        for (const auto& [name, queue] : {
                 std::pair{"advance", &battle->advance_schedule},
                 std::pair{"ready", &battle->ready_schedule},
                 std::pair{"retaliation", &battle->retaliation_schedule}}) {
            out << name << " read=" << unsigned(queue->read) << " write=" << unsigned(queue->write) << ':';
            for (unsigned i = queue->read; i != queue->write; i = (i + 1U) & 255U)
                out << ' ' << unsigned(queue->entries[i].value_or(255));
            out << '\n';
        }
        for (unsigned slot = 0; slot < 10; ++slot) {
            out << "participant=" << slot << " hp=" << battle->stats[slot].hp.current
                << " status=" << battle->status.targets[slot].active
                << " present=" << battle->status.targets[slot].present
                << " atb=" << battle->turns[slot].atb << '\n';
            for (const auto& [name, chain] : {
                     std::pair{"normal", &battle->turns[slot].normal},
                     std::pair{"retaliation", &battle->turns[slot].retaliation}}) {
                out << name << ':';
                for (const auto& command : *chain)
                    out << ' ' << unsigned(command.command) << '/' << unsigned(command.attack)
                        << '/' << command.parameters;
                out << '\n';
            }
        }
    }
    return out.str();
}
} // namespace ff6
