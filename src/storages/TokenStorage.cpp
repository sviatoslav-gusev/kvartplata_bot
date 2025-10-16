#include "TokenStorage.h"
#include "../Logger.h"

#include <array>
#include <cstddef>      // std::byte
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>


bool kbot::TokenStorage::read_all_bytes(std::string_view path,
                                        std::vector<std::byte> & out) const
{
    std::error_code ec;
    const std::uintmax_t sz = std::filesystem::file_size(path, ec);
    if (ec) {
        m_log.error("{}: cannot read filesize. Error: ", __func__, ec.message());
        return false;
    }
    std::ifstream ifs(path.data(), std::ios::binary);
    if (!ifs) {
        m_log.error("{}: cannot open file. Error: ", __func__);
        return false;
    }

    out.resize(static_cast<size_t>(sz));
    auto buf = std::as_writable_bytes(std::span{out});

    return static_cast<bool>(ifs.read(reinterpret_cast<char*>(buf.data()),
                             static_cast<std::streamsize>(buf.size()))
    );
}

bool kbot::TokenStorage::write_all_bytes(std::string_view path_to,
                                         std::span<const std::byte> data_from) const
{
    std::ofstream ofs(path_to.data(), std::ios::binary | std::ios::trunc);
    if (!ofs) {
        m_log.error("{}: cannot open file. Error: ", __func__);
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(data_from.data()),
              static_cast<std::streamsize>(data_from.size()));
#ifndef _WIN32
    // best-effort: 0600
    std::error_code ec;
    std::filesystem::permissions(
        path_to,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, ec);
#endif
    return static_cast<bool>(ofs);
}

// Fowler–Noll–Vo hash
uint64_t kbot::TokenStorage::fnv1a64(std::span<const std::byte> s) const
{
    uint64_t h = 1469598103934665603ull;
    for (std::byte c : s) {
        h ^= std::to_integer<uint64_t>(c);
        h *= 1099511628211ull;
    }
    return h;
}

// Splitmix64 hash
uint64_t kbot::TokenStorage::mix64(uint64_t x) const
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

std::string kbot::TokenStorage::system_fingerprint() const
{
    std::string fingerprint;

    auto add_env = [&](const char* name, std::string_view tag) {
        if (const char* name_val = std::getenv(name); name_val && *name_val) {
            fingerprint.append(tag).append(name_val).push_back(';');
        }
    };

#ifdef _WIN32
    add_env("USERNAME", "u:");
    add_env("COMPUTERNAME", "h:");
#else
    add_env("USER", "u:");
    add_env("HOSTNAME", "h:");
    // best-effort machine-id
    std::ifstream mid("/etc/machine-id");
    if (mid) {
        std::string id;
        std::getline(mid, id);
        if (!id.empty()) {
            fingerprint.append("mid:").append(id).push_back(';');
        }
    }
#endif

#if defined(__APPLE__)
    fingerprint += "os:mac;";
#elif defined(_WIN32)
    fingerprint += "os:win;";
#elif defined(__linux__)
    fingerprint += "os:linux;";
#else
    fingerprint += "os:unk;";
#endif
    return fingerprint;
}

std::array<uint64_t,4> kbot::TokenStorage::derive_seed(std::span<const std::byte> user_key,
                                                       std::span<const std::byte> nonce16,
                                                       std::string_view fingerprint) const
{
    const uint64_t a0 = fnv1a64(user_key);
    const uint64_t b0 = fnv1a64(nonce16);
    const uint64_t c0 = fnv1a64(std::as_bytes(std::span{fingerprint.data(), fingerprint.size()}));

    const uint64_t a = mix64(a0 ^ (b0 + 0x9e37));
    const uint64_t b = mix64(b0 ^ (c0 + 0x9e37));
    const uint64_t c = mix64(c0 ^ (a  + 0x9e37));
    const uint64_t d = mix64(a ^ b ^ c);
    return {a, b, c, d};
}

void kbot::TokenStorage::xor_keystream_inplace(std::string & data,
                                               const std::array<uint64_t, 4> & seed) const
{
    std::seed_seq seq{
        (uint32_t)(seed[0]), (uint32_t)(seed[0]>>32),
        (uint32_t)(seed[1]), (uint32_t)(seed[1]>>32),
        (uint32_t)(seed[2]), (uint32_t)(seed[2]>>32),
        (uint32_t)(seed[3]), (uint32_t)(seed[3]>>32)
    };
    std::mt19937_64 rng(seq);
    for (size_t i = 0; i < data.size();) {
        const uint64_t r = rng();
        for (int k = 0; k < 8 && i < data.size(); ++k, ++i) {
            data[i] = char(unsigned(data[i]) ^ unsigned((r >> (k*8)) & 0xFF));
        }
    }
}

bool kbot::TokenStorage::ensure_user_key() const
{
    if (std::filesystem::exists(m_paths.token_key_path())) {
        return true;
    }
    std::array<std::byte, 32> key{};
    std::random_device rd;
    for (std::byte & b : key) {
        b = static_cast<std::byte>(rd());
    }
    return write_all_bytes(m_paths.token_key_path(), key);
}

std::optional<std::vector<std::byte>> kbot::TokenStorage::load_user_key() const
{
    std::vector<std::byte> v;
    if (!read_all_bytes(m_paths.token_key_path(), v) || v.empty()) {
        return std::nullopt;
    }
    return v;
}

bool kbot::TokenStorage::write_token(std::string_view token) const
{
    // save keypart
    if (!ensure_user_key()) {
        return false;
    }

    std::optional<std::vector<std::byte>> key_opt = load_user_key();
    if (!key_opt) {
        return false;
    }
    const auto& user_key = *key_opt;

    // 16-byte nonce
    std::array<std::byte, 16> nonce{};
    std::random_device rd;
    for (std::byte & b : nonce) {
        b = static_cast<std::byte>(rd());
    }

    const std::string fingerprint = system_fingerprint();
    const std::array<uint64_t,4> seed = derive_seed(user_key, nonce, fingerprint);

    std::string cipher(token);
    xor_keystream_inplace(cipher, seed);

    // build: MAGIC | nonce | cipher
    std::string encoded_token;
    encoded_token.reserve(MAGIC.size() + nonce.size() + cipher.size());
    encoded_token.append(MAGIC.data(), MAGIC.size())
                 .append(reinterpret_cast<const char*>(nonce.data()), nonce.size())
                 .append(cipher);

    // save encoded part
    return write_all_bytes(m_paths.token_enc_path(), std::as_bytes(std::span{encoded_token.data(), encoded_token.size()}));
}

std::optional<std::string> kbot::TokenStorage::read_token() const
{
    std::optional<std::vector<std::byte>> key_opt = load_user_key();
    if (!key_opt) {
        return std::nullopt;
    }
    const std::vector<std::byte> & user_key = *key_opt;

    std::vector<std::byte> blob;
    if (!read_all_bytes(m_paths.token_enc_path(), blob)) {
        return std::nullopt;
    }

    if (blob.size() < MAGIC.size() + 16) {
        return std::nullopt;
    }

    // check MAGIC
    std::string_view head(reinterpret_cast<const char*>(blob.data()), MAGIC.size());
    if (head != MAGIC) {
        return std::nullopt;
    }

    // parse nonce + cipher
    const auto* base = blob.data();
    std::span<const std::byte> nonce(base + MAGIC.size(), 16);
    const char* cipher_ptr = reinterpret_cast<const char*>(base + MAGIC.size() + 16);
    const size_t cipher_len = blob.size() - (MAGIC.size() + 16);

    std::string cipher(cipher_ptr, cipher_len);

    const std::string fp = system_fingerprint();
    const auto seed = derive_seed(user_key, nonce, fp);

    xor_keystream_inplace(cipher, seed);
    return cipher;
}

std::string kbot::TokenStorage::load_token() const
{
    std::optional<std::string> token = read_token();
    if (token.has_value() && !token->empty())
    {
        return std::move(token.value());
    }

    m_log.wow("Token does not exist here. Input it:");
    std::string new_token;
    std::getline(std::cin, new_token);
    if (!write_token(new_token)) {
        m_log.error("Cannot write token");
        throw std::runtime_error("Cannot write token");
    }
    return new_token;
}