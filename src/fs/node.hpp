#pragma once

#include <string_view>

namespace nxmount::fs {

enum class Type {
    File,
    Directory,

    Invalid,
};
    
class INode {
public:
    [[nodiscard]] virtual auto getType() const -> Type = 0;
    [[nodiscard]] virtual auto getName() const -> std::string_view = 0;
};

} // namespace nxmount::fs