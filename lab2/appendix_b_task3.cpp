/*
Приложение Б (обязательное)
Код для задания 3

Как собрать:
clang++ -std=c++17 -O2 appendix_b_task3.cpp -o appendix_b_task3

Как запустить:
./appendix_b_task3

Что вводить:
- имя входного текстового файла
- имя выходного замаскированного файла
*/

#include <bitset>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

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

int main() {
    string input_file_name;
    string output_file_name;

    cout << "Введите имя входного текстового файла: ";
    getline(cin >> ws, input_file_name);
    cout << "Введите имя выходного (замаскированного) файла: ";
    getline(cin >> ws, output_file_name);

    ifstream input_file(input_file_name, ios::binary);
    if (!input_file.is_open()) {
        cerr << "Не удалось открыть входной файл\n";
        return 1;
    }

    ofstream output_file(output_file_name, ios::binary);
    if (!output_file.is_open()) {
        cerr << "Не удалось открыть выходной файл\n";
        return 1;
    }

    uint16_t rlz1 = RLZ1_INIT;
    bitset<100> rlz2(RLZ2_HIGH_INIT);
    rlz2 <<= 64;
    rlz2 |= bitset<100>(RLZ2_LOW_INIT);

    uint64_t byte_open[256] = {0};
    uint64_t byte_masked[256] = {0};

    string open_preview;
    string masked_preview;
    uint64_t file_bytes = 0;

    char input_byte;
    while (input_file.get(input_byte)) {
        uint8_t open_byte = static_cast<uint8_t>(input_byte);
        byte_open[open_byte]++;
        file_bytes++;

        if (open_preview.size() < 200) {
            if (open_byte >= 32 && open_byte <= 126) open_preview += static_cast<char>(open_byte);
            else open_preview += '.';
        }

        uint8_t gamma_byte = 0;
        for (int i = 0; i < 8; i++) {
            uint8_t bit = generate_combined_bit(rlz1, rlz2);
            gamma_byte |= static_cast<uint8_t>(bit << i);
        }

        uint8_t masked_byte = static_cast<uint8_t>(open_byte ^ gamma_byte);
        output_file.put(static_cast<char>(masked_byte));
        byte_masked[masked_byte]++;

        if (masked_preview.size() < 200) {
            if (masked_byte >= 32 && masked_byte <= 126) masked_preview += static_cast<char>(masked_byte);
            else masked_preview += '.';
        }
    }

    cout << "\nРазмер входного файла: " << file_bytes << " байт\n";
    if (file_bytes < 20480) {
        cout << "Внимание: по методичке желательно >= 20 Кб\n";
    }
    cout << "Файл после маскирования: " << output_file_name << "\n";

    cout << "\nФрагмент открытого текста:\n" << open_preview << "\n";
    cout << "\nФрагмент замаскированного текста:\n" << masked_preview << "\n";

    cout << "\nЧастоты байт (open_count, masked_count, open_p, masked_p):\n";
    for (int i = 0; i < 256; i++) {
        double p_open = 0.0;
        double p_masked = 0.0;
        if (file_bytes) {
            p_open = static_cast<double>(byte_open[i]) / static_cast<double>(file_bytes);
            p_masked = static_cast<double>(byte_masked[i]) / static_cast<double>(file_bytes);
        }

        cout << i << "  "
             << byte_open[i] << "  "
             << byte_masked[i] << "  "
             << p_open << "  "
             << p_masked << "\n";
    }

    return 0;
}
