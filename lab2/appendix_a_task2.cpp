/*
Приложение А (обязательное)
Код для задания 2

Как собрать:
clang++ -std=c++17 -O2 appendix_a_task2.cpp -o appendix_a_task2

Как запустить:
./appendix_a_task2

Что вводить:
- объем выборки ПСП в битах, не меньше 64
*/

#include <bitset>
#include <cmath>
#include <cstdint>
#include <iostream>

using namespace std;

constexpr int RLZ1_HIGH_BIT = 11;
constexpr uint16_t RLZ1_INIT = 0xD4B;
constexpr uint64_t RLZ2_HIGH_INIT = 0xABFCBBAEAULL;
constexpr uint64_t RLZ2_LOW_INIT = 0xAAFEBBECFEBAAFCAULL;

uint8_t generate_combined_bit(uint16_t& rlz1, bitset<100>& rlz2) {
    uint16_t feedback_bit_1 = (rlz1 & 0x1) ^ ((rlz1 >> 1) & 0x1);
    rlz1 = static_cast<uint16_t>((rlz1 >> 1) | (feedback_bit_1 << RLZ1_HIGH_BIT));

    uint8_t feedback_bit_2 = static_cast<uint8_t>(rlz2[0] ^ rlz2[36]);
    rlz2 >>= 1;
    rlz2.set(99, feedback_bit_2);

    return static_cast<uint8_t>(feedback_bit_1 ^ feedback_bit_2);
}

int count_ones(uint64_t value, int length) {
    int ones = 0;
    for (int bit = 0; bit < length; bit++) {
        ones += static_cast<int>((value >> bit) & 0x1ULL);
    }
    return ones;
}

int main() {
    uint64_t counter_psp;
    cout << "Введите объем выборки ПСП (бит, минимум 64): ";
    cin >> counter_psp;
    if (counter_psp < 64) return 1;

    uint16_t rlz1 = RLZ1_INIT;
    bitset<100> rlz2(RLZ2_HIGH_INIT);
    rlz2 <<= 64;
    rlz2 |= bitset<100>(RLZ2_LOW_INIT);

    uint64_t pfix[5][16] = {0};
    uint64_t akf31[32][2] = {0};

    long long s = 0;
    uint64_t gamma = 0;

    for (uint64_t t = 0; t < counter_psp; t++) {
        uint8_t bit = generate_combined_bit(rlz1, rlz2);
        gamma = (gamma << 1) | bit;

        for (int len = 1; len <= 4; len++) {
            if (t >= static_cast<uint64_t>(len - 1)) {
                pfix[len][gamma & ((1ULL << len) - 1)]++;
            }
        }

        if (t >= 31) {
            for (int j = 0; j < 32; j++) {
                akf31[j][(gamma & 0x1ULL) ^ ((gamma >> j) & 0x1ULL)]++;
            }
        }

        s += 2LL * static_cast<long long>(bit) - 1LL;
    }

    cout << "\nВероятности комбинаций длиной 1..4:\n";
    for (int len = 1; len <= 4; len++) {
        uint64_t den = counter_psp - static_cast<uint64_t>(len - 1);
        cout << "\nДлина " << len << ":\n";
        for (uint64_t k = 0; k < (1ULL << len); k++) {
            cout << bitset<4>(k).to_string().substr(4 - len) << " : "
                 << static_cast<double>(pfix[len][k]) / static_cast<double>(den) << "\n";
        }
    }

    cout << "\nРаспределение по числу единиц (для длин 2..4):\n";
    for (int len = 2; len <= 4; len++) {
        double r[5] = {0, 0, 0, 0, 0};
        uint64_t den = counter_psp - static_cast<uint64_t>(len - 1);

        for (uint64_t k = 0; k < (1ULL << len); k++) {
            int ones = count_ones(k, len);
            r[ones] += static_cast<double>(pfix[len][k]) / static_cast<double>(den);
        }

        cout << "\nДлина " << len << ":\n";
        for (int ones = 0; ones <= len; ones++) {
            cout << "единиц=" << ones << " : " << r[ones] << "\n";
        }
    }

    cout << "\nАКФ ПСП для tau=0..31:\n";
    for (int i = 0; i < 32; i++) {
        double k = 0.0;
        if (akf31[i][0] + akf31[i][1]) {
            k = static_cast<double>(
                    static_cast<long long>(akf31[i][0]) -
                    static_cast<long long>(akf31[i][1])) /
                static_cast<double>(akf31[i][0] + akf31[i][1]);
        }
        cout << "tau=" << i << "  K=" << k << "\n";
    }

    double sobs = static_cast<double>(abs(s)) / sqrt(static_cast<double>(counter_psp));
    double pvalue = erfc(sobs / sqrt(2.0));

    cout << "\nЧастотный тест NIST: S=" << s
         << "  Sobs=" << sobs
         << "  Pvalue=" << pvalue;
    if (pvalue > 0.01) cout << "  (пройден)\n";
    else cout << "  (не пройден)\n";

    return 0;
}
