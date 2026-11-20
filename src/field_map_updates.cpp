#include "ff6/field_map_updates.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void validate_layout(const MapLayout& layout) {
    const auto valid = [](unsigned n) { return n == 16 || n == 32 || n == 64 || n == 128; };
    if (!valid(layout.width) || !valid(layout.height))
        throw std::invalid_argument("Invalid field map publication extent");
}
MapMetatile sample(const FieldMap& map, std::uint8_t layer, unsigned x, unsigned y) {
    const auto layout = map.definition.layouts[layer];
    validate_layout(layout);
    return field_map_metatile(
        map, layer, field_map_tile(map, layer, x & (layout.width - 1U), y & (layout.height - 1U)));
}
void sample_rows(FieldMapWindow& target, const FieldMap& map, std::uint8_t layer,
                 const std::array<std::uint8_t, 2>& center, bool second, unsigned rows) {
    const auto layout = map.definition.layouts[layer];
    validate_layout(layout);
    const unsigned left = (unsigned{center[0]} + 249U) & (layout.width - 1U);
    const unsigned top = (unsigned{center[1]} + (second ? 1U : 249U)) & (layout.height - 1U);
    for (unsigned row = 0; row < rows; ++row) {
        const auto y = (top + row) & (layout.height - 1U);
        for (unsigned column = 0; column < 16; ++column) {
            const auto x = (left + column) & (layout.width - 1U);
            target[(y & 15U) * 16U + (x & 15U)] = sample(map, layer, x, y);
        }
    }
}
} // namespace
FieldMapCenters field_map_centers(const FieldScroll& scroll) {
    FieldMapCenters result;
    for (std::size_t layer = 0; layer < 3; ++layer)
        result[layer] = {scroll.layers[layer][0].center, scroll.layers[layer][1].center};
    return result;
}
FieldMapUpdates::FieldMapUpdates(const FieldMap& map, const FieldMapCenters& centers,
                                 FieldMapWorkspace retained)
    : workspace_(std::move(retained)) {
    for (std::uint8_t layer = 0; layer < 3; ++layer) {
        sample_rows(views_[layer][0], map, layer, centers[layer], false, 16);
        // Initial construction prepares/publishes sixteen rows, leaving only
        // its last row in this shared work area. Other cells retain prior state.
        const unsigned y =
            (unsigned{centers[layer][1]} + 8U) & (map.definition.layouts[layer].height - 1U);
        for (unsigned dx = 0; dx < 16; ++dx) {
            const auto x = unsigned{centers[layer][0]} + 249U + dx;
            workspace_.rows[layer][x & 15U] = sample(map, layer, x, y);
        }
        row_targets_[layer] = {0, static_cast<std::uint8_t>(y & 15U)};
    }
}
void FieldMapUpdates::request(std::uint8_t layer, bool immediate) {
    if (layer >= 3)
        throw std::invalid_argument("Invalid field map update layer");
    auto& stage = stages_[layer];
    if (immediate)
        stage = FieldMapUpdate::prepare_first;
    else if (stage != FieldMapUpdate::prepare_first)
        stage = FieldMapUpdate::deferred;
}
void FieldMapUpdates::apply_deferred() {
    for (auto& stage : stages_)
        if (stage == FieldMapUpdate::deferred)
            stage = FieldMapUpdate::prepare_first;
}
void FieldMapUpdates::notify_scroll(const FieldScrollCrossings& crossings) {
    // A crossing replaces even a prepared edge; there is one pending status
    // per axis, not an accumulating queue of crossed tiles.
    for (std::size_t layer = 0; layer < 3; ++layer)
        for (std::size_t axis = 0; axis < 2; ++axis)
            if (crossings[layer][axis])
                edges_[layer][axis] = FieldEdgeUpdate::prepare;
}
void FieldMapUpdates::prepare(const FieldMap& map, const FieldMapCenters& centers) {
    auto next = *this;
    for (std::uint8_t layer = 0; layer < 3; ++layer) {
        auto& stage = next.stages_[layer];
        if (stage == FieldMapUpdate::prepare_first || stage == FieldMapUpdate::prepare_second) {
            const bool second = stage == FieldMapUpdate::prepare_second;
            sample_rows(next.workspace_.rows[layer], map, layer, centers[layer], second, 8);
            next.row_targets_[layer] = {static_cast<std::uint8_t>(next.active_[layer] ^ 1U), 0};
            stage = second ? FieldMapUpdate::publish_first : FieldMapUpdate::prepare_second;
        }
    }
    *this = std::move(next);
}
void FieldMapUpdates::prepare_edge(const FieldMap& map, const FieldScroll& scroll,
                                   std::uint8_t layer, std::uint8_t axis) {
    const auto& velocity = scroll.layers[layer][axis];
    // Direction is selected now from the wrapped rate sum, not saved when the
    // crossing occurred. A stopped rate still selects the positive edge.
    const auto sum =
        wrap16(static_cast<std::uint32_t>(int{velocity.follow_rate} + velocity.background_rate));
    const unsigned edge = unsigned{velocity.center} + (signed16(sum) < 0 ? 249U : 8U);
    const auto other = scroll.layers[layer][axis ^ 1U].center;
    for (unsigned i = 0; i < 16; ++i) {
        const auto position = unsigned{other} + 249U + i;
        if (axis == 0)
            workspace_.columns[layer][position & 15U] = sample(map, layer, edge, position);
        else
            workspace_.rows[layer][position & 15U] = sample(map, layer, position, edge);
    }
    auto& target = axis == 0 ? column_targets_[layer] : row_targets_[layer];
    target = {active_[layer], static_cast<std::uint8_t>(edge & 15U)};
    edges_[layer][axis] = FieldEdgeUpdate::publish;
}
void FieldMapUpdates::prepare_scroll(const FieldMap& map, const FieldScroll& scroll) {
    auto next = *this;
    const auto prepare = [&](std::uint8_t layer, std::uint8_t axis) {
        if (next.edges_[layer][axis] != FieldEdgeUpdate::prepare)
            return false;
        next.prepare_edge(map, scroll, layer, axis);
        return true;
    };
    // BG3 favors vertical preparation, whereas its publication favors horizontal.
    if (!prepare(2, 1))
        prepare(2, 0);
    prepare(0, 0);
    if (!prepare(1, 0)) {
        prepare(0, 1);
        prepare(1, 1);
    }
    *this = std::move(next);
}
void FieldMapUpdates::copy_rows(std::uint8_t layer, unsigned source_row, unsigned rows) {
    const auto target = row_targets_[layer];
    // Full edits and vertical edges share this destination. Preserve a carry
    // into the next alternative/layer among the six typed background views;
    // writes beyond those views need separately recovered graphics effects.
    for (unsigned row = 0; row < rows; ++row) {
        const unsigned destination = unsigned{target.coordinate} + source_row + row;
        const unsigned view = unsigned{layer} * 2U + target.page + destination / 16U;
        if (view >= 6)
            throw std::invalid_argument(
                "Field row publication reaches unresolved graphics storage");
        for (unsigned x = 0; x < 16; ++x)
            views_[view / 2U][view % 2U][(destination % 16U) * 16U + x] =
                workspace_.rows[layer][(source_row + row) * 16U + x];
    }
}
void FieldMapUpdates::publish() {
    auto next = *this;
    for (std::uint8_t layer = 0; layer < 3; ++layer) {
        auto& stage = next.stages_[layer];
        if (stage != FieldMapUpdate::publish_first && stage != FieldMapUpdate::publish_second)
            continue;
        const bool second = stage == FieldMapUpdate::publish_second;
        next.copy_rows(layer, second ? 8U : 0U, 8);
        stage = second ? FieldMapUpdate::ready : FieldMapUpdate::publish_second;
        if (second)
            next.completed_[layer] = true;
        break;
    }
    // Flip selection, rather than copying a supposedly complete pending view:
    // an intervening edge may have published into a different alternative.
    if (std::ranges::all_of(next.stages_,
                            [](auto stage) { return stage == FieldMapUpdate::ready; }))
        for (std::size_t layer = 0; layer < 3; ++layer)
            if (next.completed_[layer]) {
                next.active_[layer] ^= 1U;
                next.completed_[layer] = false;
            }
    *this = std::move(next);
}
void FieldMapUpdates::publish_edge(std::uint8_t layer, std::uint8_t axis) {
    if (axis == 1)
        copy_rows(layer, 0, 1);
    else {
        const auto target = column_targets_[layer];
        for (unsigned y = 0; y < 16; ++y)
            views_[layer][target.page][y * 16U + target.coordinate] = workspace_.columns[layer][y];
    }
    edges_[layer][axis] = FieldEdgeUpdate::ready;
}
void FieldMapUpdates::publish_scroll() {
    auto next = *this;
    const auto publish = [&](std::uint8_t layer, std::uint8_t axis) {
        if (next.edges_[layer][axis] != FieldEdgeUpdate::publish)
            return false;
        next.publish_edge(layer, axis);
        return true;
    };
    publish(0, 0);
    if (!publish(1, 0)) {
        publish(0, 1);
        publish(1, 1);
    }
    if (!publish(2, 0))
        publish(2, 1);
    *this = std::move(next);
}
void FieldMapUpdates::publish_frame() {
    auto next = *this;
    next.publish();
    next.publish_scroll();
    *this = std::move(next);
}
bool FieldMapUpdates::ready() const {
    return std::ranges::all_of(stages_, [](auto stage) {
        return stage == FieldMapUpdate::ready || stage == FieldMapUpdate::deferred;
    });
}
std::array<std::uint8_t, 3> FieldMapUpdates::status() const {
    return {static_cast<std::uint8_t>(stages_[0]), static_cast<std::uint8_t>(stages_[1]),
            static_cast<std::uint8_t>(stages_[2])};
}
FieldMapView FieldMapUpdates::visible() const {
    FieldMapView result;
    for (std::size_t layer = 0; layer < 3; ++layer)
        for (std::size_t cell = 0; cell < 256; ++cell) {
            const auto& value = views_[layer][active_[layer]][cell];
            if (!value)
                throw std::invalid_argument(
                    "Field view needs unresolved retained preparation state");
            result[layer][cell] = *value;
        }
    return result;
}
bool consume_field_map_edit(FieldMap& map, FieldMapUpdates& updates, FieldEventRunner& runner,
                            EventFlagAccess& flags) {
    if (!runner.pending_request())
        throw std::invalid_argument("No pending field map edit");
    const auto& operation = runner.pending_request()->operation;
    if (const auto* edit = std::get_if<EventMapEdit>(&operation)) {
        auto next = map;
        auto publication = updates;
        apply_field_map_patch(next, edit->layer, edit->x, edit->y,
                              {edit->width, edit->height, edit->tiles});
        publication.request(edit->layer, edit->immediate);
        map = std::move(next);
        updates = std::move(publication);
    } else if (std::holds_alternative<EventApplyMapEdits>(operation))
        updates.apply_deferred();
    else
        return false;
    runner.complete_request(runner.pending_request()->identity, flags);
    return true;
}
} // namespace ff6
