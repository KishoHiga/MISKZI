#include <cstdint>
#include <iomanip>
#include <iostream>

namespace {

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

void expand_round_keys_256(const volatile uint32_t test_key[8], volatile uint32_t round_keys[32]) {
    for (size_t i = 0; i < 24; ++i) {
        round_keys[i] = test_key[i % 8];
    }
    for (size_t i = 0; i < 8; ++i) {
        round_keys[24 + i] = test_key[7 - i];
    }
}

void print_round_line(const char* prefix, size_t round_idx, uint32_t x1, uint32_t x0) {
    std::cout << prefix << ' ' << std::dec << (round_idx + 1) << ": "
              << "A1=" << std::hex << std::setw(8) << std::setfill('0') << x1 << ' '
              << "A0=" << std::hex << std::setw(8) << std::setfill('0') << x0 << '\n';
}

}  // namespace

int main() {
    constexpr uint64_t kPlain = 0xFEDCBA9876543210ULL;
    constexpr uint64_t kExpectedCipher = 0x4EE901E5C2D8CA3DULL;

    volatile uint32_t test_key[8] = {
        0xFFEEDDCCU,
        0xBBAA9988U,
        0x77665544U,
        0x33221100U,
        0xF0F1F2F3U,
        0xF4F5F6F7U,
        0xF8F9FAFBU,
        0xFCFDFEFFU,
    };

    volatile uint8_t s[4][256] = {};
    build_combined_s(s);

    volatile uint32_t round_keys[32] = {};
    expand_round_keys_256(test_key, round_keys);

    volatile uint32_t a1 = static_cast<uint32_t>(kPlain >> 32U);
    volatile uint32_t a0 = static_cast<uint32_t>(kPlain & 0xFFFFFFFFULL);

    std::cout << "Open block: " << std::hex << std::setw(16) << std::setfill('0') << kPlain << '\n';
    std::cout << "a1=" << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32_t>(a1)
              << " a0=" << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32_t>(a0)
              << "\n\n";

    std::cout << "Encryption rounds:\n";
    for (size_t i = 0; i < 32; ++i) {
        round_transform(&a0, &a1, &round_keys[i], s);
        print_round_line("Round", i, static_cast<uint32_t>(a1), static_cast<uint32_t>(a0));
    }

    const uint64_t cipher = (static_cast<uint64_t>(a0) << 32U) | static_cast<uint64_t>(a1);
    std::cout << "Cipher block: " << std::hex << std::setw(16) << std::setfill('0') << cipher << '\n';

    const bool enc_ok = (cipher == kExpectedCipher);
    std::cout << "Control example (encrypt): " << (enc_ok ? "OK" : "FAIL") << "\n\n";

    volatile uint32_t b1 = static_cast<uint32_t>(cipher >> 32U);
    volatile uint32_t b0 = static_cast<uint32_t>(cipher & 0xFFFFFFFFULL);

    std::cout << "Decryption rounds:\n";
    for (int i = 31, round = 1; i >= 0; --i, ++round) {
        round_transform(&b0, &b1, &round_keys[i], s);
        std::cout << "Round " << std::dec << round << ": "
                  << "B1=" << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32_t>(b1)
                  << ' ' << "B0=" << std::hex << std::setw(8) << std::setfill('0')
                  << static_cast<uint32_t>(b0) << '\n';
    }

    const volatile uint32_t tmp = b0;
    b0 = b1;
    b1 = tmp;

    const uint64_t plain_after = (static_cast<uint64_t>(b1) << 32U) | static_cast<uint64_t>(b0);
    const bool dec_ok = (plain_after == kPlain);

    std::cout << "Open block after decrypt: " << std::hex << std::setw(16) << std::setfill('0')
              << plain_after << '\n';
    std::cout << "Control example (decrypt): " << (dec_ok ? "OK" : "FAIL") << '\n';

    return (enc_ok && dec_ok) ? 0 : 1;
}
