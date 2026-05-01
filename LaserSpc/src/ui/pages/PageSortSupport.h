#pragma once

#include <array>
#include <optional>
#include <QString>

namespace LaserSpc::Ui {

template <typename EnumT, std::size_t N>
std::optional<EnumT> sortKeyForColumn(int column, const std::array<std::pair<int, EnumT>, N>& mapping) {
    for (const auto& entry : mapping) {
        if (entry.first == column) {
            return entry.second;
        }
    }
    return std::nullopt;
}

}  // namespace LaserSpc::Ui
