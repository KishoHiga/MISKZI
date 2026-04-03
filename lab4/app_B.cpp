#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs {
using path = std::string;

inline size_t file_size(const path& p) {
    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if (!in) {
        throw std::runtime_error("cannot open file");
    }
    const auto size_raw = in.tellg();
    if (size_raw < 0) {
        throw std::runtime_error("cannot determine file size");
    }
    return static_cast<size_t>(size_raw);
}
}  // namespace fs

namespace {

constexpr size_t kBlockSize = 8;
constexpr size_t kWarnLimitBytes = 10 * 1024;
constexpr size_t kBlockLimitBytes = 20 * 1024;

constexpr uint8_t PI[8][16] = {
    {12, 4, 6, 2, 10, 5, 11, 9, 14, 8, 13, 7, 0, 3, 15, 1},
    {6, 8, 2, 3, 9, 10, 5, 12, 1, 14, 4, 7, 11, 13, 0, 15},
    {11, 3, 5, 8, 2, 15, 10, 13, 14, 1, 7, 4, 12, 9, 6, 0},
    {12, 8, 2, 1, 13, 4, 15, 6, 7, 0, 10, 5, 3, 14, 9, 11},
    {7, 15, 5, 10, 8, 1, 6, 13, 0, 9, 3, 14, 11, 4, 2, 12},
    {5, 13, 15, 6, 9, 2, 12, 10, 11, 7, 8, 1, 4, 3, 14, 0},
    {8, 14, 2, 5, 6, 9, 1, 12, 15, 4, 11, 0, 13, 10, 3, 7},
    {1, 7, 14, 13, 0, 5, 8, 3, 4, 15, 10, 6, 9, 12, 11, 2},
};

struct PrngState {
    uint16_t rlz1 = 0x33A5;
    uint64_t rlz2[2] = {0x0000013ECC33AAE8ULL, 0xF999CC33999D8E8CULL};
};

struct KeyUsageState {
    size_t encrypted_bytes = 0;
    bool blocked = false;
};

uint8_t generate_bit(PrngState& state) {
    const uint16_t bit_gen1 = static_cast<uint16_t>((state.rlz1 & 0x1U) ^ ((state.rlz1 >> 1U) & 0x1U));
    state.rlz1 = static_cast<uint16_t>((state.rlz1 >> 1U) | (bit_gen1 << 13U));

    const uint64_t bit_gen2 = (state.rlz2[1] & 0x1ULL) ^ ((state.rlz2[1] >> 16U) & 0x1ULL);
    state.rlz2[1] = (state.rlz2[1] >> 1U) | ((state.rlz2[0] & 0x1ULL) << 63U);
    state.rlz2[0] = (state.rlz2[0] >> 1U) | (bit_gen2 << 40U);

    return static_cast<uint8_t>(bit_gen1 ^ bit_gen2);
}

uint8_t prng_next_u8(PrngState& state) {
    uint8_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= static_cast<uint8_t>(generate_bit(state) << i);
    }
    return value;
}

uint32_t prng_next_u32(PrngState& state) {
    uint32_t value = 0;
    for (size_t i = 0; i < 32; ++i) {
        value |= static_cast<uint32_t>(generate_bit(state)) << i;
    }
    return value;
}

void burn(volatile void* ptr, size_t len, PrngState& state) {
    if (ptr == nullptr || len == 0) {
        return;
    }

    auto* p = static_cast<volatile uint8_t*>(ptr);
    for (size_t i = 0; i < len; ++i) {
        p[i] = prng_next_u8(state);
    }
}

void burn(void* ptr, size_t len, PrngState& state) {
    burn(static_cast<volatile void*>(ptr), len, state);
}

void build_combined_s(volatile uint8_t s[4][256]) {
    for (size_t i = 0; i < 256; ++i) {
        const uint8_t hi = static_cast<uint8_t>((i >> 4U) & 0x0F);
        const uint8_t lo = static_cast<uint8_t>(i & 0x0F);

        s[0][i] = static_cast<uint8_t>((PI[1][hi] << 4U) | PI[0][lo]);
        s[1][i] = static_cast<uint8_t>((PI[3][hi] << 4U) | PI[2][lo]);
        s[2][i] = static_cast<uint8_t>((PI[5][hi] << 4U) | PI[4][lo]);
        s[3][i] = static_cast<uint8_t>((PI[7][hi] << 4U) | PI[6][lo]);
    }
}

void round_transform(volatile uint32_t* a0,
                     volatile uint32_t* a1,
                     const volatile uint32_t* round_key,
                     const volatile uint8_t s[4][256]) {
    if (a0 == nullptr || a1 == nullptr || round_key == nullptr || s == nullptr) {
        return;
    }

    const volatile uint32_t sum = (*a0) + (*round_key);

    volatile uint32_t t = 0;
    t |= static_cast<uint32_t>(s[0][(sum >> 0U) & 0xFFU]) << 0U;
    t |= static_cast<uint32_t>(s[1][(sum >> 8U) & 0xFFU]) << 8U;
    t |= static_cast<uint32_t>(s[2][(sum >> 16U) & 0xFFU]) << 16U;
    t |= static_cast<uint32_t>(s[3][(sum >> 24U) & 0xFFU]) << 24U;

    const volatile uint32_t rot = (t << 11U) | (t >> (32U - 11U));

    const uint32_t result = (*a1) ^ rot;
    *a1 = *a0;
    *a0 = result;
}

uint32_t read_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24U) | (static_cast<uint32_t>(p[1]) << 16U) |
           (static_cast<uint32_t>(p[2]) << 8U) | static_cast<uint32_t>(p[3]);
}

void write_be32(uint8_t* p, uint32_t value) {
    p[0] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
    p[1] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    p[2] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    p[3] = static_cast<uint8_t>(value & 0xFFU);
}

void expand_round_keys_32(volatile uint32_t key, volatile uint32_t round_keys[32]) {
    for (size_t i = 0; i < 32; ++i) {
        round_keys[i] = key;
    }
}

void encrypt_block(const uint8_t* in,
                   uint8_t* out,
                   const volatile uint32_t round_keys[32],
                   const volatile uint8_t s[4][256]) {
    volatile uint32_t a1 = read_be32(in + 0);
    volatile uint32_t a0 = read_be32(in + 4);

    for (size_t i = 0; i < 32; ++i) {
        round_transform(&a0, &a1, &round_keys[i], s);
    }

    write_be32(out + 0, static_cast<uint32_t>(a0));
    write_be32(out + 4, static_cast<uint32_t>(a1));
}

void decrypt_block(const uint8_t* in,
                   uint8_t* out,
                   const volatile uint32_t round_keys[32],
                   const volatile uint8_t s[4][256]) {
    volatile uint32_t b1 = read_be32(in + 0);
    volatile uint32_t b0 = read_be32(in + 4);

    for (int i = 31; i >= 0; --i) {
        round_transform(&b0, &b1, &round_keys[i], s);
    }

    const volatile uint32_t tmp = b0;
    b0 = b1;
    b1 = tmp;

    write_be32(out + 0, static_cast<uint32_t>(b1));
    write_be32(out + 4, static_cast<uint32_t>(b0));
}

bool read_binary_file(const fs::path& file_path, std::vector<uint8_t>& data) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open file for read: " << file_path << '\n';
        return false;
    }

    in.seekg(0, std::ios::end);
    const auto size_raw = in.tellg();
    if (size_raw < 0) {
        std::cerr << "Cannot determine file size: " << file_path << '\n';
        return false;
    }
    const size_t size = static_cast<size_t>(size_raw);
    in.seekg(0, std::ios::beg);

    data.assign(size, 0);
    if (size > 0) {
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        if (!in) {
            std::cerr << "Read error: " << file_path << '\n';
            return false;
        }
    }

    return true;
}

bool write_binary_file(const fs::path& file_path, const std::vector<uint8_t>& data) {
    std::ofstream out(file_path, std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open file for write: " << file_path << '\n';
        return false;
    }

    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!out) {
            std::cerr << "Write error: " << file_path << '\n';
            return false;
        }
    }

    return true;
}

void pad_proc2(std::vector<uint8_t>& data) {
    const size_t remainder = data.size() % kBlockSize;
    const size_t padding_len = (remainder == 0) ? kBlockSize : (kBlockSize - remainder);

    data.push_back(0x80);
    for (size_t i = 1; i < padding_len; ++i) {
        data.push_back(0x00);
    }
}

size_t unpad_proc2_size(const std::vector<uint8_t>& data) {
    if (data.empty() || (data.size() % kBlockSize) != 0) {
        return data.size();
    }

    size_t i = data.size();
    while (i > 0 && data[i - 1] == 0x00) {
        --i;
    }
    if (i == 0 || data[i - 1] != 0x80) {
        return data.size();
    }

    return i - 1;
}

bool has_extension(const fs::path& path, const std::string& ext) {
    if (ext.empty() || path.size() < ext.size()) {
        return false;
    }
    return path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

bool check_key_limit(size_t bytes_to_encrypt, KeyUsageState& usage) {
    if (usage.blocked) {
        std::cerr << "BLOCKED: key lifetime already exceeded." << '\n';
        return false;
    }

    if (usage.encrypted_bytes + bytes_to_encrypt > kBlockLimitBytes) {
        std::cerr << "BLOCKED: encrypting more than 20 KB with one key is forbidden." << '\n';
        usage.blocked = true;
        return false;
    }

    if (usage.encrypted_bytes <= kWarnLimitBytes && usage.encrypted_bytes + bytes_to_encrypt > kWarnLimitBytes) {
        std::cerr << "WARNING: more than 10 KB encrypted with one key. Key rotation is recommended." << '\n';
    }

    return true;
}

bool encrypt_file(const fs::path& in_path,
                  const fs::path& out_path,
                  const volatile uint32_t round_keys[32],
                  const volatile uint8_t s[4][256],
                  KeyUsageState& usage,
                  PrngState& prng) {
    if (usage.blocked) {
        std::cerr << "Program is blocked." << '\n';
        return false;
    }

    std::vector<uint8_t> plain;
    if (!read_binary_file(in_path, plain)) {
        return false;
    }

    const size_t plain_size = plain.size();
    if (!check_key_limit(plain_size, usage)) {
        if (!plain.empty()) {
            burn(plain.data(), plain.size(), prng);
        }
        return false;
    }

    pad_proc2(plain);

    std::vector<uint8_t> cipher(plain.size(), 0);
    for (size_t off = 0; off < plain.size(); off += kBlockSize) {
        encrypt_block(plain.data() + off, cipher.data() + off, round_keys, s);
    }

    const bool ok = write_binary_file(out_path, cipher);
    if (ok) {
        usage.encrypted_bytes += plain_size;
        std::cout << "Encrypted: " << in_path << " -> " << out_path << " (" << plain_size << " bytes)" << '\n';
    }

    if (!plain.empty()) {
        burn(plain.data(), plain.size(), prng);
    }
    if (!cipher.empty()) {
        burn(cipher.data(), cipher.size(), prng);
    }

    return ok;
}

bool decrypt_file(const fs::path& in_path,
                  const fs::path& out_path,
                  const volatile uint32_t round_keys[32],
                  const volatile uint8_t s[4][256],
                  PrngState& prng) {
    std::vector<uint8_t> cipher;
    if (!read_binary_file(in_path, cipher)) {
        return false;
    }

    if ((cipher.size() % kBlockSize) != 0) {
        std::cerr << "Ciphertext size must be multiple of 8 bytes: " << in_path << '\n';
        if (!cipher.empty()) {
            burn(cipher.data(), cipher.size(), prng);
        }
        return false;
    }

    std::vector<uint8_t> plain(cipher.size(), 0);
    for (size_t off = 0; off < cipher.size(); off += kBlockSize) {
        decrypt_block(cipher.data() + off, plain.data() + off, round_keys, s);
    }

    const size_t unpadded_size = unpad_proc2_size(plain);
    plain.resize(unpadded_size);

    const bool ok = write_binary_file(out_path, plain);
    if (ok) {
        std::cout << "Decrypted: " << in_path << " -> " << out_path << " (" << unpadded_size << " bytes)" << '\n';
    }

    if (!plain.empty()) {
        burn(plain.data(), plain.size(), prng);
    }
    if (!cipher.empty()) {
        burn(cipher.data(), cipher.size(), prng);
    }

    return ok;
}

bool parse_size_t(const std::string& text, size_t& out) {
    if (text.empty()) {
        return false;
    }

    size_t value = 0;
    for (char ch : text) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
        const size_t digit = static_cast<size_t>(ch - '0');
        if (value > (std::numeric_limits<size_t>::max() - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
    }

    out = value;
    return true;
}

bool parse_hex_block_16(const std::string& text, std::array<uint8_t, 8>& block) {
    if (text.size() != 16) {
        return false;
    }

    auto hex_val = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };

    for (size_t i = 0; i < 8; ++i) {
        const int hi = hex_val(text[2 * i]);
        const int lo = hex_val(text[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        block[i] = static_cast<uint8_t>((hi << 4) | lo);
    }

    return true;
}

bool generate_key_file(const fs::path& key_path, PrngState& prng) {
    if (!has_extension(key_path, ".key")) {
        std::cerr << "Key file should use .key extension." << '\n';
        return false;
    }

    uint32_t key = prng_next_u32(prng);

    std::ofstream out(key_path, std::ios::binary);
    if (!out) {
        std::cerr << "Cannot create key file: " << key_path << '\n';
        burn(&key, sizeof(key), prng);
        return false;
    }

    out.write(reinterpret_cast<const char*>(&key), static_cast<std::streamsize>(sizeof(key)));
    if (!out) {
        std::cerr << "Cannot write key file: " << key_path << '\n';
        burn(&key, sizeof(key), prng);
        return false;
    }

    std::cout << "Key generated: " << key_path << " value=0x" << std::hex << std::setw(8)
              << std::setfill('0') << key << std::dec << '\n';

    burn(&key, sizeof(key), prng);
    return true;
}

bool load_key_file(const fs::path& key_path, volatile uint32_t& key, PrngState& prng) {
    if (!has_extension(key_path, ".key")) {
        std::cerr << "Key file should use .key extension." << '\n';
        return false;
    }

    std::ifstream in(key_path, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open key file: " << key_path << '\n';
        return false;
    }

    in.seekg(0, std::ios::end);
    const auto key_size_raw = in.tellg();
    if (key_size_raw < 0) {
        std::cerr << "Cannot determine key file size: " << key_path << '\n';
        return false;
    }

    const size_t key_size = static_cast<size_t>(key_size_raw);
    if (key_size != sizeof(uint32_t)) {
        std::cerr << "Invalid key size: expected 4 bytes, got " << key_size << '\n';
        return false;
    }

    in.seekg(0, std::ios::beg);
    uint32_t temp_key = 0;
    in.read(reinterpret_cast<char*>(&temp_key), static_cast<std::streamsize>(sizeof(temp_key)));
    if (!in) {
        std::cerr << "Cannot read key file: " << key_path << '\n';
        burn(&temp_key, sizeof(temp_key), prng);
        return false;
    }

    key = temp_key;
    std::cout << "Key loaded: 0x" << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32_t>(key)
              << std::dec << '\n';

    burn(&temp_key, sizeof(temp_key), prng);
    return true;
}

bool mutate_drop_byte(const fs::path& in_path, const fs::path& out_path, size_t pos) {
    std::vector<uint8_t> data;
    if (!read_binary_file(in_path, data)) {
        return false;
    }

    if (data.size() < 3 || pos == 0 || pos >= data.size() - 1) {
        std::cerr << "Invalid byte position for drop-byte mutation." << '\n';
        return false;
    }

    data.erase(data.begin() + static_cast<std::ptrdiff_t>(pos));
    const bool ok = write_binary_file(out_path, data);
    if (ok) {
        std::cout << "Mutation drop-byte: " << out_path << '\n';
    }
    return ok;
}

bool mutate_drop_block(const fs::path& in_path, const fs::path& out_path, size_t block_idx) {
    std::vector<uint8_t> data;
    if (!read_binary_file(in_path, data)) {
        return false;
    }

    if ((data.size() % kBlockSize) != 0) {
        std::cerr << "Ciphertext is not aligned to blocks." << '\n';
        return false;
    }

    const size_t blocks = data.size() / kBlockSize;
    if (blocks < 3 || block_idx == 0 || block_idx >= blocks - 1) {
        std::cerr << "Invalid block index for drop-block mutation." << '\n';
        return false;
    }

    const size_t start = block_idx * kBlockSize;
    const size_t end = start + kBlockSize;
    data.erase(data.begin() + static_cast<std::ptrdiff_t>(start), data.begin() + static_cast<std::ptrdiff_t>(end));

    const bool ok = write_binary_file(out_path, data);
    if (ok) {
        std::cout << "Mutation drop-block: " << out_path << '\n';
    }
    return ok;
}

bool mutate_add_block(const fs::path& in_path,
                      const fs::path& out_path,
                      size_t insert_block_idx,
                      const std::array<uint8_t, 8>& new_block) {
    std::vector<uint8_t> data;
    if (!read_binary_file(in_path, data)) {
        return false;
    }

    if ((data.size() % kBlockSize) != 0) {
        std::cerr << "Ciphertext is not aligned to blocks." << '\n';
        return false;
    }

    const size_t blocks = data.size() / kBlockSize;
    if (blocks < 2 || insert_block_idx == 0 || insert_block_idx >= blocks) {
        std::cerr << "Invalid block index for add-block mutation." << '\n';
        return false;
    }

    const size_t offset = insert_block_idx * kBlockSize;
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(offset), new_block.begin(), new_block.end());

    const bool ok = write_binary_file(out_path, data);
    if (ok) {
        std::cout << "Mutation add-block: " << out_path << '\n';
    }
    return ok;
}

bool mutate_swap_blocks(const fs::path& in_path, const fs::path& out_path, size_t block_i, size_t block_j) {
    std::vector<uint8_t> data;
    if (!read_binary_file(in_path, data)) {
        return false;
    }

    if ((data.size() % kBlockSize) != 0) {
        std::cerr << "Ciphertext is not aligned to blocks." << '\n';
        return false;
    }

    const size_t blocks = data.size() / kBlockSize;
    if (blocks < 4 || block_i == block_j || block_i == 0 || block_j == 0 || block_i >= blocks - 1 ||
        block_j >= blocks - 1) {
        std::cerr << "Invalid block indexes for swap-block mutation." << '\n';
        return false;
    }

    for (size_t b = 0; b < kBlockSize; ++b) {
        std::swap(data[block_i * kBlockSize + b], data[block_j * kBlockSize + b]);
    }

    const bool ok = write_binary_file(out_path, data);
    if (ok) {
        std::cout << "Mutation swap-blocks: " << out_path << '\n';
    }
    return ok;
}

void print_usage(const char* exe) {
    std::cout << "Usage:\n"
              << "  " << exe << " gen <key.key>\n"
              << "  " << exe << " enc <key.key> <input_file> <out.enc>\n"
              << "  " << exe << " dec <key.key> <input.enc> <out.txt>\n"
              << "  " << exe << " mut-del-byte <in.enc> <out.enc> <byte_pos>\n"
              << "  " << exe << " mut-del-block <in.enc> <out.enc> <block_index>\n"
              << "  " << exe << " mut-add-block <in.enc> <out.enc> <block_index> <hex16bytes>\n"
              << "  " << exe << " mut-swap-blocks <in.enc> <out.enc> <block_i> <block_j>\n"
              << "  " << exe << " task3 <key.key> <input_10_to_20_kb> <output_prefix>\n"
              << "  " << exe << " task4 <key.key> <input_gt_20_kb> <out.enc>\n";
}

bool run_task3(const fs::path& key_path,
               const fs::path& input_path,
               const std::string& prefix,
               volatile uint32_t& key,
               volatile uint32_t round_keys[32],
               const volatile uint8_t s[4][256],
               KeyUsageState& usage,
               PrngState& prng) {
    size_t input_size = 0;
    try {
        input_size = static_cast<size_t>(fs::file_size(input_path));
    } catch (...) {
        std::cerr << "Cannot read input file size for task3: " << input_path << '\n';
        return false;
    }

    if (!(input_size > 10 * 1024 && input_size < 20 * 1024)) {
        std::cerr << "Task3 note: recommended input size is >10 KB and <20 KB. Current size=" << input_size
                  << " bytes." << '\n';
    }

    if (!load_key_file(key_path, key, prng)) {
        return false;
    }
    expand_round_keys_32(key, round_keys);

    const fs::path base_enc = prefix + ".enc";
    const fs::path base_txt = prefix + ".txt";

    if (!encrypt_file(input_path, base_enc, round_keys, s, usage, prng)) {
        return false;
    }
    if (!decrypt_file(base_enc, base_txt, round_keys, s, prng)) {
        return false;
    }

    std::vector<uint8_t> enc_data;
    if (!read_binary_file(base_enc, enc_data)) {
        return false;
    }
    if (enc_data.size() < 3 * kBlockSize || (enc_data.size() % kBlockSize) != 0) {
        std::cerr << "Ciphertext is too small for all task3 mutations." << '\n';
        return false;
    }

    const size_t blocks = enc_data.size() / kBlockSize;
    const size_t byte_pos = enc_data.size() / 2;

    size_t drop_block_idx = blocks / 2;
    if (drop_block_idx == 0) {
        drop_block_idx = 1;
    }
    if (drop_block_idx >= blocks - 1) {
        drop_block_idx = blocks - 2;
    }

    size_t add_block_idx = drop_block_idx;
    if (add_block_idx == 0) {
        add_block_idx = 1;
    }

    size_t swap_i = 1;
    size_t swap_j = blocks - 2;
    if (swap_i == swap_j) {
        swap_j = (blocks > 3) ? 2 : 1;
    }

    std::array<uint8_t, 8> random_block = {};
    for (size_t i = 0; i < random_block.size(); ++i) {
        random_block[i] = prng_next_u8(prng);
    }

    const fs::path drop_byte_enc = prefix + "_drop_byte.enc";
    const fs::path drop_block_enc = prefix + "_drop_block.enc";
    const fs::path add_block_enc = prefix + "_add_block.enc";
    const fs::path swap_blocks_enc = prefix + "_swap_blocks.enc";

    bool ok = true;
    ok &= mutate_drop_byte(base_enc, drop_byte_enc, byte_pos);
    ok &= mutate_drop_block(base_enc, drop_block_enc, drop_block_idx);
    ok &= mutate_add_block(base_enc, add_block_enc, add_block_idx, random_block);
    ok &= mutate_swap_blocks(base_enc, swap_blocks_enc, swap_i, swap_j);

    if (!ok) {
        return false;
    }

    const bool drop_byte_dec_ok = decrypt_file(drop_byte_enc, prefix + "_drop_byte.txt", round_keys, s, prng);
    if (!drop_byte_dec_ok) {
        std::cout << "Task3 note: drop-byte ciphertext is not block-aligned and cannot be decrypted in ECB." << '\n';
    }

    bool dec_ok = true;
    dec_ok &= decrypt_file(drop_block_enc, prefix + "_drop_block.txt", round_keys, s, prng);
    dec_ok &= decrypt_file(add_block_enc, prefix + "_add_block.txt", round_keys, s, prng);
    dec_ok &= decrypt_file(swap_blocks_enc, prefix + "_swap_blocks.txt", round_keys, s, prng);

    std::cout << "Task3 generated files with ciphertext distortions." << '\n';
    return dec_ok;
}

bool run_task4(const fs::path& key_path,
               const fs::path& input_path,
               const fs::path& out_path,
               volatile uint32_t& key,
               volatile uint32_t round_keys[32],
               const volatile uint8_t s[4][256],
               KeyUsageState& usage,
               PrngState& prng) {
    size_t input_size = 0;
    try {
        input_size = static_cast<size_t>(fs::file_size(input_path));
    } catch (...) {
        std::cerr << "Cannot read input file size for task4: " << input_path << '\n';
        return false;
    }

    if (!(input_size > 20 * 1024)) {
        std::cerr << "Task4 note: recommended input size is >20 KB. Current size=" << input_size << " bytes."
                  << '\n';
    }

    if (!load_key_file(key_path, key, prng)) {
        return false;
    }
    expand_round_keys_32(key, round_keys);

    const bool encrypted = encrypt_file(input_path, out_path, round_keys, s, usage, prng);
    if (!encrypted && usage.blocked) {
        std::cout << "Task4 result: program blocked as required." << '\n';
        return true;
    }

    return encrypted;
}

}  // namespace

int main(int argc, char** argv) {
    PrngState prng;
    KeyUsageState usage;

    volatile uint32_t key = 0;
    volatile uint32_t round_keys[32] = {};
    volatile uint8_t s[4][256] = {};
    build_combined_s(s);

    bool ok = false;

    if (argc < 2) {
        print_usage(argv[0]);
        ok = false;
    } else {
        const std::string cmd = argv[1];

        if (cmd == "gen" && argc == 3) {
            ok = generate_key_file(argv[2], prng);

        } else if (cmd == "enc" && argc == 5) {
            if (load_key_file(argv[2], key, prng)) {
                expand_round_keys_32(key, round_keys);
                ok = encrypt_file(argv[3], argv[4], round_keys, s, usage, prng);
            }

        } else if (cmd == "dec" && argc == 5) {
            if (load_key_file(argv[2], key, prng)) {
                expand_round_keys_32(key, round_keys);
                ok = decrypt_file(argv[3], argv[4], round_keys, s, prng);
            }

        } else if (cmd == "mut-del-byte" && argc == 5) {
            size_t pos = 0;
            if (!parse_size_t(argv[4], pos)) {
                std::cerr << "byte_pos must be a non-negative integer." << '\n';
            } else {
                ok = mutate_drop_byte(argv[2], argv[3], pos);
            }

        } else if (cmd == "mut-del-block" && argc == 5) {
            size_t block_idx = 0;
            if (!parse_size_t(argv[4], block_idx)) {
                std::cerr << "block_index must be a non-negative integer." << '\n';
            } else {
                ok = mutate_drop_block(argv[2], argv[3], block_idx);
            }

        } else if (cmd == "mut-add-block" && argc == 6) {
            size_t block_idx = 0;
            std::array<uint8_t, 8> block = {};
            if (!parse_size_t(argv[4], block_idx)) {
                std::cerr << "block_index must be a non-negative integer." << '\n';
            } else if (!parse_hex_block_16(argv[5], block)) {
                std::cerr << "hex16bytes must be exactly 16 hex symbols (8 bytes)." << '\n';
            } else {
                ok = mutate_add_block(argv[2], argv[3], block_idx, block);
            }

        } else if (cmd == "mut-swap-blocks" && argc == 6) {
            size_t block_i = 0;
            size_t block_j = 0;
            if (!parse_size_t(argv[4], block_i) || !parse_size_t(argv[5], block_j)) {
                std::cerr << "block indexes must be non-negative integers." << '\n';
            } else {
                ok = mutate_swap_blocks(argv[2], argv[3], block_i, block_j);
            }

        } else if (cmd == "task3" && argc == 5) {
            ok = run_task3(argv[2], argv[3], argv[4], key, round_keys, s, usage, prng);

        } else if (cmd == "task4" && argc == 5) {
            ok = run_task4(argv[2], argv[3], argv[4], key, round_keys, s, usage, prng);

        } else {
            print_usage(argv[0]);
            ok = false;
        }
    }

    burn(&key, sizeof(key), prng);
    burn(round_keys, sizeof(round_keys), prng);
    burn(s, sizeof(s), prng);

    return ok ? 0 : 1;
}
