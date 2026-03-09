/*
Приложение В (обязательное)
Код для задания 4

Как собрать:
clang++ -std=c++17 -O2 appendix_c_task4.cpp -o appendix_c_task4

Как запустить:
./appendix_c_task4

Что вводить:
- объем выборки линейного генератора в битах, не меньше 64
*/

#include <bitset>
#include <cstdint>
#include <iostream>

using namespace std;

constexpr uint8_t RLZ_LINEAR_INIT = 0x9; // 1001

uint8_t generate_linear_bit(uint8_t& reg) {
    uint8_t feedback_bit = static_cast<uint8_t>((reg & 0x1) ^ ((reg >> 1) & 0x1));
    reg = static_cast<uint8_t>((reg >> 1) | (feedback_bit << 3));
    return feedback_bit;
}

int main() {
    uint64_t counter_lin;
    cout << "Линейный генератор: x^4+x+1 (вариант 3)\n";
    cout << "Введите объем выборки (бит, минимум 64): ";
    cin >> counter_lin;
    if (counter_lin < 64) return 1;

    uint8_t r_start = RLZ_LINEAR_INIT;
    uint8_t r_period = r_start;
    uint64_t period = 0;

    do {
        generate_linear_bit(r_period);
        period++;
    } while (r_period != r_start);

    uint8_t rlz_lin = r_start;
    uint64_t pl[5][16] = {0};
    uint64_t akf63[64][2] = {0};
    uint64_t gamma_lin = 0;

    for (uint64_t t = 0; t < counter_lin; t++) {
        uint8_t bit = generate_linear_bit(rlz_lin);
        gamma_lin = (gamma_lin << 1) | bit;

        for (int len = 1; len <= 4; len++) {
            if (t >= static_cast<uint64_t>(len - 1)) {
                pl[len][gamma_lin & ((1ULL << len) - 1)]++;
            }
        }

        if (t >= 63) {
            for (int j = 0; j < 64; j++) {
                akf63[j][(gamma_lin & 0x1ULL) ^ ((gamma_lin >> j) & 0x1ULL)]++;
            }
        }
    }

    cout << "\nПериод: " << period << "\n";
    cout << "\nВероятности комбинаций длиной 1..4:\n";
    for (int len = 1; len <= 4; len++) {
        uint64_t den = counter_lin - static_cast<uint64_t>(len - 1);
        cout << "\nДлина " << len << ":\n";
        for (uint64_t k = 0; k < (1ULL << len); k++) {
            cout << bitset<4>(k).to_string().substr(4 - len) << " : "
                 << static_cast<double>(pl[len][k]) / static_cast<double>(den) << "\n";
        }
    }

    cout << "\nАКФ для tau=0..63:\n";
    for (int i = 0; i < 64; i++) {
        double k = 0.0;
        if (akf63[i][0] + akf63[i][1]) {
            k = static_cast<double>(
                    static_cast<long long>(akf63[i][0]) -
                    static_cast<long long>(akf63[i][1])) /
                static_cast<double>(akf63[i][0] + akf63[i][1]);
        }
        cout << "tau=" << i << "  K=" << k << "\n";
    }

    return 0;
}
