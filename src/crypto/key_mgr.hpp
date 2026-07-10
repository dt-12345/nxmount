/*
  ISC License

  Copyright (c) 2018, SciresM

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#pragma once

#include "hactool/settings.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace nxmount::crypto {

struct TitleKey {
    std::uint8_t rightsId[0x10];
    std::uint8_t titleKey[0x10];
};

class KeyManager {
public:
    auto initialize(std::string_view keyPath, std::string_view titleKeyPath) -> void;

    [[nodiscard]] static auto instance() -> KeyManager* { return std::addressof(sInstance); }

    [[nodiscard]] auto getKeySet() const -> const nca_keyset_t& { return mKeySet; }
    [[nodiscard]] auto getTitleKeys() const -> const std::vector<TitleKey>& { return mTitleKeys; }

    [[nodiscard]] auto getTitleKey(const std::uint8_t* rightsId) const -> const TitleKey*;
    [[nodiscard]] auto getExternalTitleKey() const -> const std::uint8_t(&)[0x10] { return mExternalTitleKey; }

    auto addTitleKey(const std::uint8_t* rightsId, const std::uint8_t* titleKey) -> void;
    auto setExternalTitleKey(const std::uint8_t* titleKey) -> void;

private:
    KeyManager();

    [[nodiscard]] auto getTitleKey(const std::uint8_t* rightsId) -> TitleKey*;

    static KeyManager sInstance;

    nca_keyset_t mKeySet = {};
    std::vector<TitleKey> mTitleKeys = {};
    std::uint8_t mExternalTitleKey[0x10];
};

} // namespace nxmount::crypto