/*
Приложение А (обязательное)
Код для заданий 1 и 2

Как собрать:
clang++ -std=c++17 -O2 appendix_a_task2.cpp -o appendix_a_task2

Как запустить:
./appendix_a_task2

Что вводить:
- максимальную длину комбинации n для статистики ПСП/открытых/замаскированных данных (1..8)
- максимальный сдвиг tau для АКФ этих данных (0..31)
- объем выборки ПСП в битах для задания 2 (минимум 64)
- имя входного текстового файла
- имя выходного замаскированного файла
*/

#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

constexpr int RLZ1_HIGH_BIT = 11;
constexpr uint16_t RLZ1_INIT = 0xD4B;
constexpr uint64_t RLZ2_HIGH_INIT = 0xABFCBBAEAULL;
constexpr uint64_t RLZ2_LOW_INIT = 0xAAFEBBECFEBAAFCAULL;

constexpr int TASK1_MIN_LEN = 1;
constexpr int TASK1_MAX_LEN = 8;
constexpr int TASK1_MAX_TAU = 31;
constexpr int TASK2_MAX_LEN = 4;
constexpr int TASK2_MAX_TAU = 31;

uint8_t generate_combined_bit(uint16_t& rlz1, bitset<100>& rlz2) {
    uint16_t feedback_bit_1 = (rlz1 & 0x1) ^ ((rlz1 >> 1) & 0x1);
    rlz1 = static_cast<uint16_t>((rlz1 >> 1) | (feedback_bit_1 << RLZ1_HIGH_BIT));

    uint8_t feedback_bit_2 = static_cast<uint8_t>(rlz2[0] ^ rlz2[36]);
    rlz2 >>= 1;
    rlz2.set(99, feedback_bit_2);

    return static_cast<uint8_t>(feedback_bit_1 ^ feedback_bit_2);
}

void reset_combined_registers(uint16_t& rlz1, bitset<100>& rlz2) {
    rlz1 = RLZ1_INIT;
    rlz2 = bitset<100>(RLZ2_HIGH_INIT);
    rlz2 <<= 64;
    rlz2 |= bitset<100>(RLZ2_LOW_INIT);
}

int count_ones(uint64_t value, int length) {
    int ones = 0;
    for (int bit = 0; bit < length; bit++) {
        ones += static_cast<int>((value >> bit) & 0x1ULL);
    }
    return ones;
}

string binary_string(uint64_t value, int width) {
    string result(width, '0');
    for (int bit = 0; bit < width; bit++) {
        if ((value >> bit) & 0x1ULL) {
            result[width - 1 - bit] = '1';
        }
    }
    return result;
}

double pvalue_from_sum(long long sum_s, uint64_t total_bits) {
    if (total_bits == 0) return 0.0;
    double sobs = static_cast<double>(llabs(sum_s)) /
                  sqrt(static_cast<double>(total_bits));
    return erfc(sobs / sqrt(2.0));
}

double akf_value(const array<uint64_t, 2>& counters) {
    uint64_t total = counters[0] + counters[1];
    if (total == 0) return 0.0;
    return static_cast<double>(
               static_cast<long long>(counters[0]) -
               static_cast<long long>(counters[1])) /
           static_cast<double>(total);
}

struct SequenceStats {
    int combo_len;
    int tau_max;
    uint64_t combo_mask;
    vector<uint64_t> combo_counts;
    vector<array<uint64_t, 2>> akf_counts;
    long long sum_s;
    uint64_t total_bits;
    uint64_t reg;

    SequenceStats(int combo_len_in, int tau_max_in)
        : combo_len(combo_len_in),
          tau_max(tau_max_in),
          combo_mask((1ULL << combo_len_in) - 1ULL),
          combo_counts(1ULL << combo_len_in, 0ULL),
          akf_counts(tau_max_in + 1, array<uint64_t, 2>{0ULL, 0ULL}),
          sum_s(0),
          total_bits(0),
          reg(0) {}

    void push(uint8_t bit) {
        reg = (reg << 1) | bit;

        if (total_bits >= static_cast<uint64_t>(combo_len - 1)) {
            combo_counts[reg & combo_mask]++;
        }

        for (int tau = 0; tau <= tau_max; tau++) {
            if (total_bits >= static_cast<uint64_t>(tau)) {
                akf_counts[tau][(reg & 0x1ULL) ^ ((reg >> tau) & 0x1ULL)]++;
            }
        }

        sum_s += 2LL * static_cast<long long>(bit) - 1LL;
        total_bits++;
    }

    uint64_t combo_denominator() const {
        if (total_bits < static_cast<uint64_t>(combo_len)) return 0;
        return total_bits - static_cast<uint64_t>(combo_len) + 1;
    }
};

void print_task2_statistics(uint64_t sample_size_bits) {
    uint16_t rlz1;
    bitset<100> rlz2;
    reset_combined_registers(rlz1, rlz2);

    uint64_t combo_counts[TASK2_MAX_LEN + 1][1 << TASK2_MAX_LEN] = {};
    array<array<uint64_t, 2>, TASK2_MAX_TAU + 1> akf_counts = {};

    long long sum_s = 0;
    uint64_t gamma = 0;

    for (uint64_t t = 0; t < sample_size_bits; t++) {
        uint8_t bit = generate_combined_bit(rlz1, rlz2);
        gamma = (gamma << 1) | bit;

        for (int len = 1; len <= TASK2_MAX_LEN; len++) {
            if (t >= static_cast<uint64_t>(len - 1)) {
                combo_counts[len][gamma & ((1ULL << len) - 1ULL)]++;
            }
        }

        for (int tau = 0; tau <= TASK2_MAX_TAU; tau++) {
            if (t >= static_cast<uint64_t>(tau)) {
                akf_counts[tau][(gamma & 0x1ULL) ^ ((gamma >> tau) & 0x1ULL)]++;
            }
        }

        sum_s += 2LL * static_cast<long long>(bit) - 1LL;
    }

    cout << "\n===== ЗАДАНИЕ 2 =====\n";
    cout << "Объем выборки ПСП: " << sample_size_bits << " бит\n";

    cout << "\nВероятности комбинаций длиной 1..4:\n";
    for (int len = 1; len <= TASK2_MAX_LEN; len++) {
        uint64_t den = sample_size_bits - static_cast<uint64_t>(len - 1);
        cout << "\nДлина " << len << ":\n";
        for (uint64_t combo = 0; combo < (1ULL << len); combo++) {
            cout << binary_string(combo, len) << " : "
                 << static_cast<double>(combo_counts[len][combo]) /
                        static_cast<double>(den)
                 << "\n";
        }
    }

    cout << "\nРаспределение по числу единиц (для длин 2..4):\n";
    for (int len = 2; len <= TASK2_MAX_LEN; len++) {
        uint64_t den = sample_size_bits - static_cast<uint64_t>(len - 1);
        cout << "\nДлина " << len << ":\n";
        for (int ones = 0; ones <= len; ones++) {
            uint64_t count_sum = 0;
            for (uint64_t combo = 0; combo < (1ULL << len); combo++) {
                if (count_ones(combo, len) == ones) {
                    count_sum += combo_counts[len][combo];
                }
            }

            cout << "единиц=" << ones << " : "
                 << static_cast<double>(count_sum) /
                        static_cast<double>(den)
                 << "\n";
        }
    }

    cout << "\nАКФ ПСП для tau=0..31:\n";
    for (int tau = 0; tau <= TASK2_MAX_TAU; tau++) {
        cout << "tau=" << tau << "  K=" << akf_value(akf_counts[tau]) << "\n";
    }

    double sobs = static_cast<double>(llabs(sum_s)) /
                  sqrt(static_cast<double>(sample_size_bits));
    double pvalue = erfc(sobs / sqrt(2.0));

    cout << "\nЧастотный тест NIST: S=" << sum_s
         << "  Sobs=" << sobs
         << "  Pvalue=" << pvalue;
    if (pvalue > 0.01) cout << "  (пройден)\n";
    else cout << "  (не пройден)\n";
}

bool process_file_statistics(const string& input_file_name,
                             const string& output_file_name,
                             int combo_len,
                             int tau_max) {
    ifstream input_file(input_file_name, ios::binary);
    if (!input_file.is_open()) {
        cerr << "Не удалось открыть входной файл\n";
        return false;
    }

    ofstream output_file(output_file_name, ios::binary);
    if (!output_file.is_open()) {
        cerr << "Не удалось открыть выходной файл\n";
        return false;
    }

    uint16_t rlz1;
    bitset<100> rlz2;
    reset_combined_registers(rlz1, rlz2);

    SequenceStats psp_stats(combo_len, tau_max);
    SequenceStats raw_stats(combo_len, tau_max);
    SequenceStats masked_stats(combo_len, tau_max);

    uint64_t file_bytes = 0;
    char input_char = 0;

    while (input_file.get(input_char)) {
        uint8_t open_byte = static_cast<uint8_t>(input_char);
        uint8_t masked_byte = 0;

        for (int bit_index = 0; bit_index < 8; bit_index++) {
            uint8_t gamma_bit = generate_combined_bit(rlz1, rlz2);
            uint8_t file_bit = static_cast<uint8_t>((open_byte >> bit_index) & 0x1U);
            uint8_t masked_bit = static_cast<uint8_t>(file_bit ^ gamma_bit);

            masked_byte |= static_cast<uint8_t>(masked_bit << bit_index);

            psp_stats.push(gamma_bit);
            raw_stats.push(file_bit);
            masked_stats.push(masked_bit);
        }

        output_file.put(static_cast<char>(masked_byte));
        file_bytes++;
    }

    output_file.close();

    cout << "\n===== ЗАДАНИЕ 1 =====\n";
    cout << "Статистика ПСП, открытых и замаскированных данных\n";
    cout << "Размер входного файла: " << file_bytes << " байт\n";
    cout << "Файл после маскирования: " << output_file_name << "\n";
    cout << "Длина анализируемых комбинаций: " << combo_len << " бит\n";
    cout << "Максимальный сдвиг АКФ: " << tau_max << "\n";

    cout << "\nВероятности комбинаций (ПСП, open, masked):\n";
    uint64_t combo_limit = 1ULL << combo_len;
    cout << fixed << setprecision(6);
    for (uint64_t combo = 0; combo < combo_limit; combo++) {
        double p_psp = 0.0;
        double p_raw = 0.0;
        double p_masked = 0.0;

        if (psp_stats.combo_denominator() != 0) {
            p_psp = static_cast<double>(psp_stats.combo_counts[combo]) /
                    static_cast<double>(psp_stats.combo_denominator());
        }
        if (raw_stats.combo_denominator() != 0) {
            p_raw = static_cast<double>(raw_stats.combo_counts[combo]) /
                    static_cast<double>(raw_stats.combo_denominator());
        }
        if (masked_stats.combo_denominator() != 0) {
            p_masked = static_cast<double>(masked_stats.combo_counts[combo]) /
                       static_cast<double>(masked_stats.combo_denominator());
        }

        cout << binary_string(combo, combo_len)
             << "  " << p_psp
             << "  " << p_raw
             << "  " << p_masked << "\n";
    }

    if (combo_len >= 2) {
        cout << "\nРаспределение по числу единиц (ПСП, open, masked):\n";
        for (int ones = 0; ones <= combo_len; ones++) {
            uint64_t sum_psp = 0;
            uint64_t sum_raw = 0;
            uint64_t sum_masked = 0;

            for (uint64_t combo = 0; combo < combo_limit; combo++) {
                if (count_ones(combo, combo_len) == ones) {
                    sum_psp += psp_stats.combo_counts[combo];
                    sum_raw += raw_stats.combo_counts[combo];
                    sum_masked += masked_stats.combo_counts[combo];
                }
            }

            double p_psp = 0.0;
            double p_raw = 0.0;
            double p_masked = 0.0;
            if (psp_stats.combo_denominator() != 0) {
                p_psp = static_cast<double>(sum_psp) /
                        static_cast<double>(psp_stats.combo_denominator());
            }
            if (raw_stats.combo_denominator() != 0) {
                p_raw = static_cast<double>(sum_raw) /
                        static_cast<double>(raw_stats.combo_denominator());
            }
            if (masked_stats.combo_denominator() != 0) {
                p_masked = static_cast<double>(sum_masked) /
                           static_cast<double>(masked_stats.combo_denominator());
            }

            cout << "единиц=" << ones
                 << "  " << p_psp
                 << "  " << p_raw
                 << "  " << p_masked << "\n";
        }
    }

    cout << "\nАКФ (psp, open, masked):\n";
    for (int tau = 0; tau <= tau_max; tau++) {
        cout << "tau=" << tau
             << "  " << akf_value(psp_stats.akf_counts[tau])
             << "  " << akf_value(raw_stats.akf_counts[tau])
             << "  " << akf_value(masked_stats.akf_counts[tau]) << "\n";
    }

    cout << "\nЧастотный тест NIST:\n";
    cout << "ПСП: Pvalue=" << pvalue_from_sum(psp_stats.sum_s, psp_stats.total_bits) << "\n";
    cout << "Open: Pvalue=" << pvalue_from_sum(raw_stats.sum_s, raw_stats.total_bits) << "\n";
    cout << "Masked: Pvalue=" << pvalue_from_sum(masked_stats.sum_s, masked_stats.total_bits)
         << "\n";

    cout.unsetf(ios::floatfield);
    cout << setprecision(6);
    return true;
}

int main() {
    int combo_len = 0;
    int tau_max = 0;
    uint64_t sample_size_bits = 0;
    string input_file_name;
    string output_file_name;

    cout << "Приложение А: задания 1 и 2\n";
    cout << "Введите длину комбинации n для задания 1 (1..8): ";
    cin >> combo_len;
    if (combo_len < TASK1_MIN_LEN || combo_len > TASK1_MAX_LEN) {
        cerr << "Ошибка: длина комбинации должна быть от 1 до 8.\n";
        return 1;
    }

    cout << "Введите tau_max для АКФ в задании 1 (0..31): ";
    cin >> tau_max;
    if (tau_max < 0 || tau_max > TASK1_MAX_TAU) {
        cerr << "Ошибка: tau_max должен быть от 0 до 31.\n";
        return 1;
    }

    cout << "Введите объем выборки ПСП для задания 2 (бит, минимум 64): ";
    cin >> sample_size_bits;
    if (sample_size_bits < 64) {
        cerr << "Ошибка: объем выборки должен быть не меньше 64 бит.\n";
        return 1;
    }

    cout << "Введите имя входного текстового файла: ";
    cin >> ws;
    getline(cin, input_file_name);
    cout << "Введите имя выходного (замаскированного) файла: ";
    getline(cin, output_file_name);

    if (!process_file_statistics(input_file_name, output_file_name, combo_len, tau_max)) {
        return 1;
    }

    print_task2_statistics(sample_size_bits);

    return 0;
}
