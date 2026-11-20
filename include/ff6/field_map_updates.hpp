#pragma once
#include "ff6/field_camera.hpp"
#include "ff6/field_events.hpp"

namespace ff6 {
using FieldMapCenters = std::array<std::array<std::uint8_t, 2>, 3>;
using FieldScrollCrossings = std::array<std::array<bool, 2>, 3>;
FieldMapCenters field_map_centers(const FieldScroll& scroll);
enum class FieldMapUpdate : std::uint8_t {
    ready,
    publish_second,
    publish_first,
    prepare_second,
    prepare_first,
    deferred
};
enum class FieldEdgeUpdate : std::uint8_t { ready, prepare, publish };
using FieldMapWindow = std::array<std::optional<MapMetatile>, 256>;
struct FieldMapWorkspace {
    std::array<FieldMapWindow, 3> rows;
    std::array<std::array<std::optional<MapMetatile>, 16>, 3> columns;
    bool operator==(const FieldMapWorkspace&) const = default;
};
// Game-specific row/column publication of prepared native scene cells. Logical
// terrain remains separate; no device memory, register or transfer-program API.
class FieldMapUpdates {
  public:
    FieldMapUpdates(const FieldMap& map, const FieldMapCenters& centers,
                    FieldMapWorkspace retained = {});
    const FieldMapWorkspace& workspace() const {
        return workspace_;
    }
    void request(std::uint8_t layer, bool immediate);
    void apply_deferred();
    void notify_scroll(const FieldScrollCrossings& crossings);
    // Ordinary logic prepares full edits before scrolling. Startup instead uses
    // only prepare()/publish(), without a camera advancement or display frame.
    void prepare(const FieldMap& map, const FieldMapCenters& centers);
    void prepare_scroll(const FieldMap& map, const FieldScroll& scroll);
    void publish();
    void publish_scroll();
    void publish_frame(); // Full edits, then ready scrolling edges, atomically.
    bool ready() const;
    std::array<std::uint8_t, 3> status() const;
    std::array<std::array<FieldEdgeUpdate, 2>, 3> edges() const {
        return edges_;
    }
    FieldMapView visible() const;
    bool operator==(const FieldMapUpdates&) const = default;

  private:
    struct Destination {
        std::uint8_t page = 0, coordinate = 0;
        bool operator==(const Destination&) const = default;
    };
    std::array<FieldMapUpdate, 3> stages_{};
    std::array<std::array<FieldEdgeUpdate, 2>, 3> edges_{}; // x,y
    std::array<bool, 3> completed_{};
    std::array<std::uint8_t, 3> active_{};
    std::array<std::array<FieldMapWindow, 2>, 3> views_;
    FieldMapWorkspace workspace_;
    std::array<Destination, 3> row_targets_{}, column_targets_{};
    void prepare_edge(const FieldMap& map, const FieldScroll& scroll, std::uint8_t layer,
                      std::uint8_t axis);
    void copy_rows(std::uint8_t layer, unsigned source_row, unsigned rows);
    void publish_edge(std::uint8_t layer, std::uint8_t axis);
};
bool consume_field_map_edit(FieldMap& map, FieldMapUpdates& updates, FieldEventRunner& runner,
                            EventFlagAccess& flags);
} // namespace ff6
