#pragma once

#include "PathsStorage.h"
#include "../Logger.h"
#include <span>
#include <string>
#include <optional>

/* MAGIC(5) | nonce(16) | ciphertext */

namespace kbot {

class TokenStorage
{
public:
    TokenStorage(const PathsStorage & paths, Logger & log)
        : m_paths(paths)
        , m_log(log)
    {}

    std::string load_token() const;

private:
    bool read_all_bytes(std::string_view path, std::vector<std::byte> & out) const;
    bool write_all_bytes(std::string_view path_to, std::span<const std::byte> data_from) const;

    uint64_t fnv1a64(std::span<const std::byte> s) const;
    uint64_t mix64(uint64_t x) const;
    std::string system_fingerprint() const;
    std::array<uint64_t,4> derive_seed(std::span<const std::byte> user_key,
                                       std::span<const std::byte> nonce16,
                                       std::string_view fingerprint) const;
    void xor_keystream_inplace(std::string & data,
                               const std::array<uint64_t, 4> & seed) const;
    bool ensure_user_key() const;
    std::optional<std::vector<std::byte>> load_user_key() const;

    // Single initial call
    bool write_token(std::string_view token) const;
    std::optional<std::string> read_token() const;

private:
    const PathsStorage & m_paths;
    Logger & m_log;

    static constexpr std::string_view MAGIC = "KTOK1";
};

} // namespace kbot