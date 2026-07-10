#pragma once

#include <cstdint>
#include <cerrno>

#define NO_FILE -ENOENT
#define BAD_FILE -EBADF
#define UNIMPLEMENTED -ENOSYS
#define INVALID -EINVAL
#define READ_ONLY -EROFS
#define PERMISSION_ERROR -EPERM
#define UNSUPPORTED -ENOTSUP
#define SUCCESS 0

#define SUCCEEDED(res) ((res) == SUCCESS)
#define FAILED(res) (!SUCCEEDED(res))

namespace nxmount {

using Result = std::int32_t;

} // namespace nxmount