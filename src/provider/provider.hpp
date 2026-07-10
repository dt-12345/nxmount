#pragma once

#include "common/utils.hpp"

#include <cstddef>
#include <memory>
#include <type_traits>

namespace nxmount::provider {

// a provider may read more than it actually has, it's the file's responsibility to clamp where necessary
class IBytesProvider {
public:
    virtual ~IBytesProvider() = 0;

    virtual auto getSize() const -> std::size_t = 0;

    virtual auto read(void* dst, std::size_t size, std::size_t offset) -> std::size_t = 0;
    virtual auto write(const void* src, std::size_t size) -> std::size_t = 0;

protected:
    [[nodiscard]] ALWAYS_INLINE static auto Clamp(std::size_t size, std::size_t offset, std::size_t totalSize) -> std::size_t {
        if (offset >= totalSize) {
            return 0;
        }
        if (offset + size > totalSize) {
            return totalSize - offset;
        }
        return size;
    }
};

inline IBytesProvider::~IBytesProvider() = default;

class ReadOnlyBytesProvider : public IBytesProvider {
public:
    auto write(const void*, std::size_t) -> std::size_t override final { return 0; }
};

template <typename T>
concept IsProvider = std::is_base_of_v<IBytesProvider, T>;

namespace detail {

template <typename T>
struct IsUniquePtr;

template <typename T>
struct IsUniquePtr<std::unique_ptr<T>> : std::true_type {};

template <typename T>
struct IsUniquePtr : std::false_type {};

template <typename T>
struct IsSharedPtr;

template <typename T>
struct IsSharedPtr<std::shared_ptr<T>> : std::true_type {};

template <typename T>
struct IsSharedPtr : std::false_type {};

template <typename T>
concept IsSmartPointer = detail::IsUniquePtr<T>::value || detail::IsSharedPtr<T>::value;

template <typename T> requires IsSmartPointer<T>
struct PointedToType {
    using type = T::element_type;
};

template <typename T, typename U>
concept IsPointerTo = IsSmartPointer<T> && (std::is_same_v<U, typename PointedToType<T>::type> || std::is_base_of_v<U, typename PointedToType<T>::type>);

} // namespace detail

template <typename PtrT>
concept ProviderWrapper = detail::IsPointerTo<PtrT, IBytesProvider>;

using UniqueProvider = std::unique_ptr<IBytesProvider>;
using SharedProvider = std::shared_ptr<IBytesProvider>;

} // namespace nxmount::provider