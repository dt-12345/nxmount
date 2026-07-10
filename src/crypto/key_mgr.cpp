#include "crypto/crypto.hpp"
#include "crypto/key_mgr.hpp"
#include "crypto/key_utils.hpp"
#include "hactool/pki.h"
#include "hactool/settings.h"
#include "log/logging.hpp"

#include <aes.h>
#include <modes.h>

#include <filesystem>
#include <fstream>

extern "C" {

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

// hactool crypto functions but using cryptopp instead
static void GenerateKek(std::uint8_t *dst, const std::uint8_t *src, const std::uint8_t *masterKey, const std::uint8_t *kekSeed, const std::uint8_t *keySeed) {
    std::uint8_t kek[0x10];
    std::uint8_t srcKek[0x10];
    nxmount::crypto::AesEcbDecrypt(kek, kekSeed, 0x10, masterKey, 0x10);
    nxmount::crypto::AesEcbDecrypt(srcKek, src, 0x10, kek, 0x10);

    if (keySeed != NULL) {
        nxmount::crypto::AesEcbDecrypt(dst, keySeed, 0x10, srcKek, 0x10);
    } else {
        memcpy(dst, srcKek, 0x10);
    }
}

void pki_derive_keys(nca_keyset_t *keyset) {
    std::uint8_t zeroes[0x100];
    std::uint8_t cmac[0x10];
    memset(zeroes, 0, 0x100);
    memset(cmac, 0, 0x10);
    /* Derive keys as necessary. */
    for (std::uint32_t i = 0; i < 0x6; i++) {
        /* Start by deriving keyblob keys. */
        if (!nxmount::crypto::IsNull(keyset->secure_boot_key)) {
            continue;
        }
        if (!nxmount::crypto::IsNull(keyset->tsec_key)) {
            continue;
        }
        if (!nxmount::crypto::IsNull(keyset->keyblob_key_sources[i])) {
            continue;
        }
        nxmount::crypto::AesEcbDecrypt(&keyset->keyblob_keys[i], &keyset->keyblob_key_sources[i], 0x10, keyset->tsec_key, 0x10);
        nxmount::crypto::AesEcbDecrypt(&keyset->keyblob_keys[i], &keyset->keyblob_keys[i], 0x10, keyset->secure_boot_key, 0x10);
        if (!nxmount::crypto::IsNull(keyset->keyblob_mac_key_source)) {
            continue;
        }
        nxmount::crypto::AesEcbDecrypt(&keyset->keyblob_mac_keys[i], keyset->keyblob_mac_key_source, 0x10, &keyset->keyblob_keys[i], 0x10);
        /* Derive Device key */
        if (i == 0 && nxmount::crypto::IsNull(keyset->per_console_key_source)) {
            nxmount::crypto::AesEcbDecrypt(keyset->device_key, keyset->per_console_key_source, 0x10, &keyset->keyblob_keys[i], 0x10);
        }
    }
    for (std::uint32_t i = 0; i < 0x6; i++) {
        /* Then we decrypt keyblobs. */
        if (!nxmount::crypto::IsNull(keyset->keyblob_keys[i])) {
            continue;
        }
        if (!nxmount::crypto::IsNull(keyset->keyblob_mac_keys[i])) {
            continue;
        }
        if (!nxmount::crypto::IsNull(keyset->encrypted_keyblobs[i])) {
            continue;
        }
        nxmount::crypto::AesCalculateCmac(cmac, &keyset->encrypted_keyblobs[i][0x10], 0xA0, keyset->keyblob_mac_keys[i], 0x80);
        if (memcmp(cmac, &keyset->encrypted_keyblobs[i][0], 0x10) != 0) {
            LOG_WARNING("Keyblob MAC {:02x} is invalid. Are SBK/TSEC key correct?\n", i);
            continue;
        }
        nxmount::crypto::AesCtrDecrypt(&keyset->keyblobs[i], &keyset->encrypted_keyblobs[i][0x20], sizeof(keyset->keyblobs[i]), &keyset->keyblob_keys[i], 0x10, &keyset->encrypted_keyblobs[i][0x10]);
	}
    for (std::uint32_t i = 0; i < 0x6; i++) {
        /* Set package1 key as relevant. */
        if (nxmount::crypto::IsNull(reinterpret_cast<std::uint8_t(&)[0x10]>(keyset->keyblobs[i][0x80]))) {
            memcpy(&keyset->package1_keys[i], &keyset->keyblobs[i][0x80], 0x10);
        }
        /* Set master kek as relevant. */
        if (nxmount::crypto::IsNull(reinterpret_cast<std::uint8_t(&)[0x10]>(keyset->keyblobs[i]))) {
            memcpy(&keyset->master_keks[i], &keyset->keyblobs[i][0x00], 0x10);
        }
    }
    for (std::uint32_t i = 0x6; i < 0x20; i++) {
        /* Derive new 6.2.0+ keks. */
        if (!nxmount::crypto::IsNull(keyset->tsec_auth_signatures[i-6])) {
            continue;
        }

        /* Derive TSEC root key. */
        if (nxmount::crypto::IsNull(keyset->tsec_root_kek)) {
            nxmount::crypto::AesEcbDecrypt(keyset->tsec_root_keys[i-6], keyset->tsec_auth_signatures[i-6], 0x10, keyset->tsec_root_kek, 0x10);
        }

        /* Derive package1 MAC key */
        if (nxmount::crypto::IsNull(keyset->package1_mac_kek)) {
            nxmount::crypto::AesEcbDecrypt(keyset->package1_mac_keys[i], keyset->tsec_auth_signatures[i-6], 0x10, keyset->package1_mac_kek, 0x10);
        }

        /* Derive package1 key */
        if (nxmount::crypto::IsNull(keyset->package1_kek)) {
            nxmount::crypto::AesEcbDecrypt(keyset->package1_keys[i], keyset->tsec_auth_signatures[i-6], 0x10, keyset->package1_kek, 0x10);
        }
    }
    for (std::uint32_t i = 0x6; i < 0x20; i++) {
        /* Do new keygen for 6.2.0+. */
        const std::uint32_t which_tsec_root_key = (i >= 0x8) ? (0x8 - 6) : (i - 6);
        if (!nxmount::crypto::IsNull(keyset->tsec_root_keys[which_tsec_root_key])) {
            continue;
        }
        if (!nxmount::crypto::IsNull(keyset->master_kek_sources[i])) {
            continue;
        }

        nxmount::crypto::AesEcbDecrypt(keyset->master_keks[i], keyset->master_kek_sources[i], 0x10, keyset->tsec_root_keys[which_tsec_root_key], 0x10);
    }
    /* Derive master keks with mariko keydata -- these are always preferred to other sources. */
    for (std::uint32_t i = 0; i < 0x20; i++) {
        if (!nxmount::crypto::IsNull(keyset->mariko_kek)) {
            continue;
        }
        if (!nxmount::crypto::IsNull(keyset->mariko_master_kek_sources[i])) {
            continue;
        }
        nxmount::crypto::AesEcbDecrypt(keyset->master_keks[i], keyset->mariko_master_kek_sources[i], 0x10, keyset->mariko_kek, 0x10);
    }
    for (std::uint32_t i = 0; i < 0x20; i++) {
        /* Then we derive master keys. */
        if (!nxmount::crypto::IsNull(keyset->master_key_source)) {
            continue;
        }
        /* We need a non-zero master kek. */
        if (!nxmount::crypto::IsNull(keyset->master_keks[i])) {
            continue;
        }

        /* Derive Master Keys. */
        nxmount::crypto::AesEcbDecrypt(&keyset->master_keys[i], keyset->master_key_source, 0x10, &keyset->master_keks[i], 0x10);
    }
    for (std::uint32_t i = 0; i < 0x20; i++) {
        if (!nxmount::crypto::IsNull(keyset->master_keys[i])) {
            continue;
        }

        /* Derive Key Area Encryption Keys */
        if (nxmount::crypto::IsNull(keyset->key_area_key_application_source)) {
            GenerateKek(keyset->key_area_keys[i][0], keyset->key_area_key_application_source, keyset->master_keys[i], keyset->aes_kek_generation_source, keyset->aes_key_generation_source);
        }
        if (nxmount::crypto::IsNull(keyset->key_area_key_ocean_source)) {
            GenerateKek(keyset->key_area_keys[i][1], keyset->key_area_key_ocean_source, keyset->master_keys[i], keyset->aes_kek_generation_source, keyset->aes_key_generation_source);
        }
        if (nxmount::crypto::IsNull(keyset->key_area_key_system_source)) {
            GenerateKek(keyset->key_area_keys[i][2], keyset->key_area_key_system_source, keyset->master_keys[i], keyset->aes_kek_generation_source, keyset->aes_key_generation_source);
        }

        /* Derive Titlekek */
        if (nxmount::crypto::IsNull(keyset->titlekek_source)) {
            nxmount::crypto::AesEcbDecrypt(&keyset->titlekeks[i], keyset->titlekek_source, 0x10, &keyset->master_keys[i], 0x10);
        }

        /* Derive Package2 Key */
        if (nxmount::crypto::IsNull(keyset->package2_key_source)) {
            nxmount::crypto::AesEcbDecrypt(&keyset->package2_keys[i], keyset->package2_key_source, 0x10, &keyset->master_keys[i], 0x10);
        }

        /* Derive Header Key */
        if (i == 0 && nxmount::crypto::IsNull(keyset->header_kek_source) && nxmount::crypto::IsNull(keyset->header_key_source)) {
            std::uint8_t header_kek[0x10];
            GenerateKek(header_kek, keyset->header_kek_source, keyset->master_keys[i], keyset->aes_kek_generation_source, keyset->aes_key_generation_source);
            nxmount::crypto::AesEcbDecrypt(keyset->header_key, keyset->header_key_source, 0x20, header_kek, 0x10);
        }

        /* Derive SD Card Key */
        if (i == 0 && nxmount::crypto::IsNull(keyset->sd_card_kek_source)) {
            std::uint8_t sd_kek[0x10];
            GenerateKek(sd_kek, keyset->sd_card_kek_source, keyset->master_keys[i], keyset->aes_kek_generation_source, keyset->aes_key_generation_source);

            for (std::uint32_t k = 0; k < 2; k++) {
                if (nxmount::crypto::IsNull(keyset->sd_card_key_sources[k])) {
                    nxmount::crypto::AesEcbDecrypt(keyset->sd_card_keys[k], keyset->sd_card_key_sources[k], 0x20, sd_kek, 0x10);
                }
            }
        }

        /* Derive Save MAC Key */
        if (i == 0 && nxmount::crypto::IsNull(keyset->save_mac_kek_source) && nxmount::crypto::IsNull(keyset->save_mac_key_source) && nxmount::crypto::IsNull(keyset->device_key)) {
            GenerateKek(keyset->save_mac_key, keyset->save_mac_kek_source, keyset->device_key, keyset->aes_kek_generation_source, keyset->save_mac_key_source);
        }

    }

}

} // extern "C"

namespace nxmount::crypto {

KeyManager KeyManager::sInstance = {};

[[nodiscard]] static auto GetKeyFilePath(std::string_view path, const char* defaultFilename) -> std::filesystem::path {
    if (path.empty()) {
        const char* home = ::getenv("HOME");
        if (home == nullptr) {
            home = ::getenv("USERPROFILE");
        }
        if (home == nullptr) {
            LOG_FATAL("No existing key file!");
        }
        return std::filesystem::path(home) / std::filesystem::path(".switch") / std::filesystem::path(defaultFilename);
    } else {
        return path;
    }
}

[[nodiscard]] static auto RemoveLeadingAndTrailingWhitspace(const std::string& str) -> std::string {
    const auto start = str.find_first_not_of(" \t");
    const auto end = str.find_last_not_of(" \t");
    return str.substr(start != std::string::npos ? start : 0, end != std::string::npos ? end + 1 : std::string::npos);
}

[[nodiscard]] static auto ParseKeyValue(std::ifstream& f, std::string& key, std::string& value) -> bool {
    if (std::string line; !std::getline(f, line)) {
        return false;
    } else {
        const auto pos = line.find_first_of("=,");
        if (pos == std::string::npos) {
            return false;
        }

        key = RemoveLeadingAndTrailingWhitspace(line.substr(0, pos));
        value = RemoveLeadingAndTrailingWhitspace(line.substr(pos + 1));

        return IsHexString(value);
    }
}

static auto LoadKeyFile(nca_keyset_t* keyset, const std::filesystem::path& path) -> void {
    LOG_INFO("Using key file {}", path.string());
    auto infile = std::ifstream(path);
    if (!infile) {
        LOG_WARNING("Key file {} could not be opened", path.string());
        return;
    }
    std::string key, value;
    while (ParseKeyValue(infile, key, value)) {
        if (key =="aes_kek_generation_source") {
            ParseKey(keyset->aes_kek_generation_source, sizeof(keyset->aes_kek_generation_source), value);
        } else if (key =="aes_key_generation_source") {
            ParseKey(keyset->aes_key_generation_source, sizeof(keyset->aes_key_generation_source), value);
        } else if (key =="key_area_key_application_source") {
            ParseKey(keyset->key_area_key_application_source, sizeof(keyset->key_area_key_application_source), value);
        } else if (key =="key_area_key_ocean_source") {
            ParseKey(keyset->key_area_key_ocean_source, sizeof(keyset->key_area_key_ocean_source), value);
        } else if (key =="key_area_key_system_source") {
            ParseKey(keyset->key_area_key_system_source, sizeof(keyset->key_area_key_system_source), value);
        } else if (key =="titlekek_source") {
            ParseKey(keyset->titlekek_source, sizeof(keyset->titlekek_source), value);
        } else if (key =="header_kek_source") {
            ParseKey(keyset->header_kek_source, sizeof(keyset->header_kek_source), value);
        } else if (key =="header_key_source") {
            ParseKey(keyset->header_key_source, sizeof(keyset->header_key_source), value);
        } else if (key =="header_key") {
            ParseKey(keyset->header_key, sizeof(keyset->header_key), value);
        } else if (key =="package2_key_source") {
            ParseKey(keyset->package2_key_source, sizeof(keyset->package2_key_source), value);
        } else if (key =="per_console_key_source") {
            ParseKey(keyset->per_console_key_source, sizeof(keyset->per_console_key_source), value);
        } else if (key =="xci_header_key") {
            ParseKey(keyset->xci_header_key, sizeof(keyset->xci_header_key), value);
        } else if (key =="sd_card_kek_source") {
            ParseKey(keyset->sd_card_kek_source, sizeof(keyset->sd_card_kek_source), value);
        } else if (key =="sd_card_nca_key_source") {
            ParseKey(keyset->sd_card_key_sources[1], sizeof(keyset->sd_card_key_sources[1]), value);
        } else if (key =="sd_card_save_key_source") {
            ParseKey(keyset->sd_card_key_sources[0], sizeof(keyset->sd_card_key_sources[0]), value);
        } else if (key =="save_mac_kek_source") {
            ParseKey(keyset->save_mac_kek_source, sizeof(keyset->save_mac_kek_source), value);
        }  else if (key =="save_mac_key_source") {
            ParseKey(keyset->save_mac_key_source, sizeof(keyset->save_mac_key_source), value);
        }  else if (key =="master_key_source") {
            ParseKey(keyset->master_key_source, sizeof(keyset->master_key_source), value);
        } else if (key =="keyblob_mac_key_source") {
            ParseKey(keyset->keyblob_mac_key_source, sizeof(keyset->keyblob_mac_key_source), value);
        } else if (key =="secure_boot_key") {
            ParseKey(keyset->secure_boot_key, sizeof(keyset->secure_boot_key), value);
        } else if (key =="tsec_key") {
            ParseKey(keyset->tsec_key, sizeof(keyset->tsec_key), value);
        } else if (key =="mariko_kek") {
            ParseKey(keyset->mariko_kek, sizeof(keyset->mariko_kek), value);
        }  else if (key =="mariko_bek") {
            ParseKey(keyset->mariko_bek, sizeof(keyset->mariko_bek), value);
        }  else if (key =="tsec_root_kek") {
            ParseKey(keyset->tsec_root_kek, sizeof(keyset->tsec_root_kek), value);
        } else if (key =="package1_mac_kek") {
            ParseKey(keyset->package1_mac_kek, sizeof(keyset->package1_mac_kek), value);
        } else if (key =="package1_kek") {
            ParseKey(keyset->package1_kek, sizeof(keyset->package1_kek), value);
        } else if (key =="beta_nca0_exponent") {
            std::uint8_t exponent[0x100] = {0};
            ParseKey(exponent, sizeof(exponent), value);
            pki_set_beta_nca0_exponent(exponent);
        } else {
            char test_name[0x100] = {0};
            for (std::uint32_t i = 0; i < 0x6; i++) {
                snprintf(test_name, sizeof(test_name), "keyblob_key_source_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->keyblob_key_sources[i], sizeof(keyset->keyblob_key_sources[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "keyblob_key_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->keyblob_keys[i], sizeof(keyset->keyblob_keys[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "keyblob_mac_key_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->keyblob_mac_keys[i], sizeof(keyset->keyblob_mac_keys[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "encrypted_keyblob_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->encrypted_keyblobs[i], sizeof(keyset->encrypted_keyblobs[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "mariko_master_kek_source_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->mariko_master_kek_sources[i], sizeof(keyset->mariko_master_kek_sources[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "keyblob_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->keyblobs[i], sizeof(keyset->keyblobs[i]), value);
                    break;
                }
            }
            for (std::uint32_t i = 0x6; i < 0x20; i++) {
                snprintf(test_name, sizeof(test_name), "tsec_auth_signature_%02x", i - 6);
                if (key ==test_name) {
                    ParseKey(keyset->tsec_auth_signatures[i - 6], sizeof(keyset->tsec_auth_signatures[i - 6]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "tsec_root_key_%02x", i - 6);
                if (key ==test_name) {
                    ParseKey(keyset->tsec_root_keys[i - 6], sizeof(keyset->tsec_root_keys[i - 6]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "master_kek_source_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->master_kek_sources[i], sizeof(keyset->master_kek_sources[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "mariko_master_kek_source_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->mariko_master_kek_sources[i], sizeof(keyset->mariko_master_kek_sources[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "package1_mac_key_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->package1_mac_keys[i], sizeof(keyset->package1_mac_keys[i]), value);
                    break;
                }
            }

            for (std::uint32_t i = 0; i < 0xc; i++) {
                snprintf(test_name, sizeof(test_name), "mariko_aes_class_key_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->mariko_aes_class_keys[i], sizeof(keyset->mariko_aes_class_keys[i]), value);
                    break;
                }
            }

            for (std::uint32_t i = 0; i < 0x20; i++) {
                snprintf(test_name, sizeof(test_name), "master_kek_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->master_keks[i], sizeof(keyset->master_keks[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "master_key_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->master_keys[i], sizeof(keyset->master_keys[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "package1_key_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->package1_keys[i], sizeof(keyset->package1_keys[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "package2_key_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->package2_keys[i], sizeof(keyset->package2_keys[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "titlekek_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->titlekeks[i], sizeof(keyset->titlekeks[i]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "key_area_key_application_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->key_area_keys[i][0], sizeof(keyset->key_area_keys[i][0]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "key_area_key_ocean_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->key_area_keys[i][1], sizeof(keyset->key_area_keys[i][1]), value);
                    break;
                }

                snprintf(test_name, sizeof(test_name), "key_area_key_system_%02x", i);
                if (key ==test_name) {
                    ParseKey(keyset->key_area_keys[i][2], sizeof(keyset->key_area_keys[i][2]), value);
                    break;
                }
            }
        }
    }
}

[[nodiscard]] static auto FindKey(const std::vector<TitleKey>& keys, const std::uint8_t* rightsId) -> const TitleKey* {
    for (const auto& key : keys) {
        if (std::memcmp(key.rightsId, rightsId, sizeof(key.rightsId)) == 0) {
            return std::addressof(key);
        }
    }
    return nullptr;
}

[[nodiscard]] static auto FindKey(std::vector<TitleKey>& keys, const std::uint8_t* rightsId) -> TitleKey* {
    for (auto& key : keys) {
        if (std::memcmp(key.rightsId, rightsId, sizeof(key.rightsId)) == 0) {
            return std::addressof(key);
        }
    }
    return nullptr;
}

static auto LoadTitleKeys(std::vector<TitleKey>& keys, const std::filesystem::path& path) -> void {
    LOG_INFO("Using title key file {}", path.string());
    auto infile = std::ifstream(path);
    if (!infile) {
        LOG_WARNING("Key file {} could not be opened", path.string());
        return;
    }
    std::string key, value;
    while (ParseKeyValue(infile, key, value)) {
        std::uint8_t rightsId[0x10];
        std::uint8_t titlekey[0x10];

        if (key.size() != 0x20 || !IsHexString(key)) {
            LOG_WARNING("Invalid title.keys content: \"{}\", (value \"{}\")\n", key, value);
        } else {
            ParseKey(rightsId, sizeof(rightsId), key);
            ParseKey(titlekey, sizeof(titlekey), value);
            if (const auto existing = FindKey(keys, rightsId); existing != nullptr) {
                LOG_WARNING("Ignoring duplicate title in title.keys!");
            } else {
                auto& newKey = keys.emplace_back();
                std::memcpy(newKey.rightsId, rightsId, sizeof(newKey.rightsId));
                std::memcpy(newKey.titleKey, titlekey, sizeof(newKey.titleKey));
            }
        }
    }
}

auto KeyManager::initialize(std::string_view keyPath, std::string_view titleKeyPath) -> void {
    pki_initialize_keyset(std::addressof(mKeySet), KEYSET_RETAIL);
    LoadKeyFile(std::addressof(mKeySet), GetKeyFilePath(keyPath, "prod.keys"));
    LoadTitleKeys(mTitleKeys, GetKeyFilePath(titleKeyPath, "title.keys"));
    pki_derive_keys(std::addressof(mKeySet));
}

auto KeyManager::getTitleKey(const std::uint8_t* rightsId) const -> const TitleKey* {
    return FindKey(mTitleKeys, rightsId);
}

auto KeyManager::getTitleKey(const std::uint8_t* rightsId) -> TitleKey* {
    return FindKey(mTitleKeys, rightsId);
}

auto KeyManager::addTitleKey(const std::uint8_t* rightsId, const std::uint8_t* titleKey) -> void {
    if (auto existing = getTitleKey(rightsId); existing != nullptr) {
        std::memcpy(existing->titleKey, titleKey, sizeof(existing->titleKey));
    } else {
        auto& key = mTitleKeys.emplace_back();
        std::memcpy(key.rightsId, rightsId, sizeof(key.rightsId));
        std::memcpy(key.titleKey, titleKey, sizeof(key.titleKey));
    }
}

auto KeyManager::setExternalTitleKey(const std::uint8_t* titleKey) -> void {
    std::memcpy(mExternalTitleKey, titleKey, sizeof(mExternalTitleKey));
}

KeyManager::KeyManager() {
    std::memset(std::addressof(mKeySet), 0, sizeof(mKeySet));
}

} // namespace nxmount::crypto