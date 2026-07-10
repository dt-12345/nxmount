#include "driver/driver.hpp"

auto main(int argc, const char** argv) -> int {
    return nxmount::driver::Run(argc, argv);
}