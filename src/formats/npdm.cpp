#include "crypto/crypto.hpp"
#include "crypto/key_mgr.hpp"
#include "formats/npdm.hpp"
#include "log/logging.hpp"

#include <cstring>

namespace nxmount::formats {

auto GetNCAHeaderKey(provider::UniqueProvider& provider, std::uint8_t* key) -> bool {
    MetaHeader header;
    if (provider->read(std::addressof(header), sizeof(header), 0) != sizeof(header)) {
        LOG_WARNING("Failed to read NPDM META header");
        return false;
    }

    if (header.magic != MetaHeader::cMagic) {
        return false;
    }

    if (header.acidSize < sizeof(Acid)) {
        return false;
    }

    Acid acid;
    if (provider->read(std::addressof(acid), sizeof(acid), header.acidOffset) != sizeof(acid)) {
        LOG_WARNING("Failed to read NPDM ACID header");
        return false;
    }

    if (header.keyGeneration >= 2) {
        return false;
    }

    auto data = std::vector<std::uint8_t>(acid.size);
    if (provider->read(data.data(), data.size(), header.acidOffset + offsetof(Acid, modulus)) != data.size()) {
        return false;
    }

    const auto& acidKey = crypto::KeyManager::instance()->getKeySet().acid_fixed_key_moduli[header.keyGeneration];
    if (!crypto::RsaPssVerify(data.data(), data.size(), acid.signature, acidKey)) {
        return false;
    }

    std::memcpy(key, acid.modulus, sizeof(acid.modulus));
    return true;
}

} // namespace nxmount::formats