#include "formats/cnmt.hpp"
#include "log/logging.hpp"

namespace nxmount::formats {

ContentMetaReader::ContentMetaReader(provider::UniqueProvider provider) {
    mData.resize(provider->getSize());
    if (provider->read(mData.data(), mData.size(), 0) != mData.size()) {
        LOG_ERROR("Failed to read CNMT file");
        mData.clear();
        return;
    }

    if (mData.size() < sizeof(PackagedContentMetaHeader)) {
        LOG_ERROR("Failed to read PackagedContentMetaHeader");
        mData.clear();
        return;
    }
}

} // namespace nxmount::formats