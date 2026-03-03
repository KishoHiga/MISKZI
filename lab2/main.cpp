#include <cstdint>
#include <iostream>
#include <bitset>
#include <fstream>
#include <string>
#include <cmath>

uint64_t bit;

//K1(x)=x^12+x+1
int n = 12;
int i;
int j;

uint16_t Rlz1 = 0xD4B; //0000110101001011
uint16_t feedback_bit_1;

//K2(x)=x^100+x^37+1 точки съема 100 и 63
uint64_t feedback_bit_2;
std::bitset<100> Rlz2(0xABFCBBAEA); // старшие биты
std::bitset<100> Rlz2_1(0xAAFEBBECFEBAAFCA); //младшие биты

//Задание 4, вариант 3: x^4+x+1
uint8_t Rlz_lin = 0x9; //1001
uint8_t feedback_bit_lin;

int main() {
    int n_user;
    int tau_user;
    uint64_t counter_psp;
    uint64_t counter_lin;

    std::cout << "ЛР2 (все 4 задания, без графиков)\n";
    std::cout << "Введите длину комбинации n для задания 1 (1..8): ";
    std::cin >> n_user;
    if (n_user < 1 || n_user > 8) return 1;

    std::cout << "Введите tau_max для АКФ в задании 1 (0..63): ";
    std::cin >> tau_user;
    if (tau_user < 0 || tau_user > 63) return 1;

    uint64_t comb_count = (1ULL << n_user);
    uint64_t mask_user = comb_count - 1;

    // ========================
    // Задание 2
    // ========================
    std::cout << "\n===== ЗАДАНИЕ 2 =====\n";
    std::cout << "Введите объем выборки ПСП (бит, минимум 64): ";
    std::cin >> counter_psp;
    if (counter_psp < 64) return 1;

    Rlz1 = 0xD4B;
    Rlz2 = std::bitset<100>(0xABFCBBAEA);
    Rlz2 = Rlz2 << 64;
    Rlz2 = Rlz2 | Rlz2_1;

    uint64_t Pfix[5][16] = {0}; // [длина 1..4][индекс комбинации]
    uint64_t Akf31[32][2] = {0};

    uint64_t* P_user_psp = new uint64_t[comb_count]{0};
    uint64_t** Akf_user_psp = new uint64_t* [tau_user + 1];
    for (i = 0; i <= tau_user; i++) Akf_user_psp[i] = new uint64_t[2]{0, 0};

    long long S = 0;
    uint64_t gamma = 0;

    for (uint64_t t = 0; t < counter_psp; t++) {
        feedback_bit_1 = (Rlz1 & 0x1) ^ ((Rlz1 >> 1) & 0x1);
        Rlz1 = (Rlz1 >> 1) | (feedback_bit_1 << n);

        feedback_bit_2 = Rlz2[0] ^ Rlz2[36];
        Rlz2 = (Rlz2 >> 1);
        Rlz2.set(99, feedback_bit_2);

        bit = (feedback_bit_1) ^ (feedback_bit_2);
        gamma = (gamma << 1) | bit;

        for (int len = 1; len <= 4; len++) {
            if (t >= static_cast<uint64_t>(len - 1)) {
                Pfix[len][gamma & ((1ULL << len) - 1)]++;
            }
        }

        if (t >= 31) {
            for (j = 0; j < 32; j++) {
                Akf31[j][(gamma & 0x1) ^ ((gamma >> j) & 0x1)]++;
            }
        }

        if (t >= static_cast<uint64_t>(n_user - 1)) {
            P_user_psp[gamma & mask_user]++;
        }
        if (t >= static_cast<uint64_t>(tau_user)) {
            for (j = 0; j <= tau_user; j++) {
                Akf_user_psp[j][(gamma & 0x1) ^ ((gamma >> j) & 0x1)]++;
            }
        }

        S += 2 * static_cast<long long>(bit) - 1;
    }

    std::cout << "\nВероятности комбинаций длиной 1..4:\n";
    for (int len = 1; len <= 4; len++) {
        uint64_t den = counter_psp - (len - 1);
        std::cout << "\nДлина " << len << ":\n";
        for (uint64_t k = 0; k < (1ULL << len); k++) {
            std::cout << std::bitset<4>(k).to_string().substr(4 - len) << " : "
                      << static_cast<double>(Pfix[len][k]) / static_cast<double>(den) << "\n";
        }
    }

    std::cout << "\nРаспределение по числу единиц (для длин 2..4):\n";
    for (int len = 2; len <= 4; len++) {
        double R[5] = {0, 0, 0, 0, 0};
        uint64_t den = counter_psp - (len - 1);
        for (uint64_t k = 0; k < (1ULL << len); k++) {
            int ones = 0;
            for (int b = 0; b < len; b++) ones += ((k >> b) & 0x1);
            R[ones] += static_cast<double>(Pfix[len][k]) / static_cast<double>(den);
        }
        std::cout << "\nДлина " << len << ":\n";
        for (int ones = 0; ones <= len; ones++) {
            std::cout << "единиц=" << ones << " : " << R[ones] << "\n";
        }
    }

    std::cout << "\nАКФ ПСП для tau=0..31:\n";
    for (i = 0; i < 32; i++) {
        double k = 0;
        if (Akf31[i][0] + Akf31[i][1]) {
            k = static_cast<double>(static_cast<long long>(Akf31[i][0]) - static_cast<long long>(Akf31[i][1])) /
                static_cast<double>(Akf31[i][0] + Akf31[i][1]);
        }
        std::cout << "tau=" << i << "  K=" << k << "\n";
    }

    double Sobj = static_cast<double>(std::abs(S)) / std::sqrt(static_cast<double>(counter_psp));
    double Pval = std::erfc(Sobj / std::sqrt(2.0));
    std::cout << "\nЧастотный тест NIST: S=" << S << "  Sobs=" << Sobj << "  Pvalue=" << Pval;
    if (Pval > 0.01) std::cout << "  (пройден)\n";
    else std::cout << "  (не пройден)\n";

    std::cout << "\nСтатистика ПСП для задания 1 (n=" << n_user << ", tau_max=" << tau_user << "):\n";
    uint64_t user_den_psp = counter_psp - static_cast<uint64_t>(n_user - 1);
    std::cout << "Вероятности комбинаций:\n";
    for (uint64_t k = 0; k < comb_count; k++) {
        std::cout << std::bitset<8>(k).to_string().substr(8 - n_user) << " : "
                  << static_cast<double>(P_user_psp[k]) / static_cast<double>(user_den_psp) << "\n";
    }
    std::cout << "АКФ:\n";
    for (i = 0; i <= tau_user; i++) {
        double k = 0;
        if (Akf_user_psp[i][0] + Akf_user_psp[i][1]) {
            k = static_cast<double>(static_cast<long long>(Akf_user_psp[i][0]) - static_cast<long long>(Akf_user_psp[i][1])) /
                static_cast<double>(Akf_user_psp[i][0] + Akf_user_psp[i][1]);
        }
        std::cout << "tau=" << i << "  K=" << k << "\n";
    }

    for (i = 0; i <= tau_user; i++) delete[] Akf_user_psp[i];
    delete[] Akf_user_psp;
    delete[] P_user_psp;

    // ========================
    // Задание 3
    // ========================
    std::cout << "\n===== ЗАДАНИЕ 3 =====\n";
    std::string input_file_name;
    std::string output_file_name;

    std::cout << "Введите имя входного текстового файла: ";
    std::getline(std::cin >> std::ws, input_file_name);
    std::cout << "Введите имя выходного (замаскированного) файла: ";
    std::getline(std::cin >> std::ws, output_file_name);

    std::ifstream input_file(input_file_name, std::ios::binary);
    if (!input_file.is_open()) return 1;
    std::ofstream output_file(output_file_name, std::ios::binary);
    if (!output_file.is_open()) return 1;

    Rlz1 = 0xD4B;
    Rlz2 = std::bitset<100>(0xABFCBBAEA);
    Rlz2 = Rlz2 << 64;
    Rlz2 = Rlz2 | Rlz2_1;

    uint64_t byte_open[256] = {0};
    uint64_t byte_masked[256] = {0};

    uint64_t* P_user_open = new uint64_t[comb_count]{0};
    uint64_t* P_user_masked = new uint64_t[comb_count]{0};

    uint64_t** Akf_open = new uint64_t* [tau_user + 1];
    uint64_t** Akf_masked = new uint64_t* [tau_user + 1];
    for (i = 0; i <= tau_user; i++) {
        Akf_open[i] = new uint64_t[2]{0, 0};
        Akf_masked[i] = new uint64_t[2]{0, 0};
    }

    std::string open_preview;
    std::string masked_preview;

    uint64_t bits_open = 0;
    uint64_t bits_masked = 0;
    uint64_t gamma_open = 0;
    uint64_t gamma_masked = 0;
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
        for (i = 0; i < 8; i++) {
            feedback_bit_1 = (Rlz1 & 0x1) ^ ((Rlz1 >> 1) & 0x1);
            Rlz1 = (Rlz1 >> 1) | (feedback_bit_1 << n);

            feedback_bit_2 = Rlz2[0] ^ Rlz2[36];
            Rlz2 = (Rlz2 >> 1);
            Rlz2.set(99, feedback_bit_2);

            bit = (feedback_bit_1) ^ (feedback_bit_2);
            gamma_byte = gamma_byte | (static_cast<uint8_t>(bit) << i);
        }

        uint8_t masked_byte = open_byte ^ gamma_byte;
        output_file.put(static_cast<char>(masked_byte));
        byte_masked[masked_byte]++;

        if (masked_preview.size() < 200) {
            if (masked_byte >= 32 && masked_byte <= 126) masked_preview += static_cast<char>(masked_byte);
            else masked_preview += '.';
        }

        for (i = 0; i < 8; i++) {
            uint64_t b1 = (open_byte >> i) & 0x1;
            gamma_open = (gamma_open << 1) | b1;
            if (bits_open >= static_cast<uint64_t>(n_user - 1)) P_user_open[gamma_open & mask_user]++;
            if (bits_open >= static_cast<uint64_t>(tau_user)) {
                for (j = 0; j <= tau_user; j++) Akf_open[j][(gamma_open & 0x1) ^ ((gamma_open >> j) & 0x1)]++;
            }
            bits_open++;

            uint64_t b2 = (masked_byte >> i) & 0x1;
            gamma_masked = (gamma_masked << 1) | b2;
            if (bits_masked >= static_cast<uint64_t>(n_user - 1)) P_user_masked[gamma_masked & mask_user]++;
            if (bits_masked >= static_cast<uint64_t>(tau_user)) {
                for (j = 0; j <= tau_user; j++) Akf_masked[j][(gamma_masked & 0x1) ^ ((gamma_masked >> j) & 0x1)]++;
            }
            bits_masked++;
        }
    }

    std::cout << "\nРазмер входного файла: " << file_bytes << " байт\n";
    if (file_bytes < 20480) std::cout << "Внимание: по методичке желательно >= 20 Кб\n";
    std::cout << "Файл после маскирования: " << output_file_name << "\n";

    std::cout << "\nФрагмент открытого текста:\n" << open_preview << "\n";
    std::cout << "\nФрагмент замаскированного текста:\n" << masked_preview << "\n";

    std::cout << "\nЧастоты байт (open_count, masked_count, open_p, masked_p):\n";
    for (i = 0; i < 256; i++) {
        double p_open = 0;
        double p_mask = 0;
        if (file_bytes) {
            p_open = static_cast<double>(byte_open[i]) / static_cast<double>(file_bytes);
            p_mask = static_cast<double>(byte_masked[i]) / static_cast<double>(file_bytes);
        }
        std::cout << i << "  " << byte_open[i] << "  " << byte_masked[i] << "  " << p_open << "  " << p_mask << "\n";
    }

    uint64_t user_den_open = 0;
    uint64_t user_den_masked = 0;
    if (bits_open >= static_cast<uint64_t>(n_user - 1)) user_den_open = bits_open - static_cast<uint64_t>(n_user - 1);
    if (bits_masked >= static_cast<uint64_t>(n_user - 1)) user_den_masked = bits_masked - static_cast<uint64_t>(n_user - 1);

    std::cout << "\nЗадание 1 для открытых/замаскированных данных:\n";
    std::cout << "Вероятности комбинаций:\n";
    for (uint64_t k = 0; k < comb_count; k++) {
        double p1 = 0;
        double p2 = 0;
        if (user_den_open) p1 = static_cast<double>(P_user_open[k]) / static_cast<double>(user_den_open);
        if (user_den_masked) p2 = static_cast<double>(P_user_masked[k]) / static_cast<double>(user_den_masked);
        std::cout << std::bitset<8>(k).to_string().substr(8 - n_user) << "  " << p1 << "  " << p2 << "\n";
    }

    std::cout << "АКФ (open, masked):\n";
    for (i = 0; i <= tau_user; i++) {
        double k1 = 0;
        double k2 = 0;
        if (Akf_open[i][0] + Akf_open[i][1]) {
            k1 = static_cast<double>(static_cast<long long>(Akf_open[i][0]) - static_cast<long long>(Akf_open[i][1])) /
                 static_cast<double>(Akf_open[i][0] + Akf_open[i][1]);
        }
        if (Akf_masked[i][0] + Akf_masked[i][1]) {
            k2 = static_cast<double>(static_cast<long long>(Akf_masked[i][0]) - static_cast<long long>(Akf_masked[i][1])) /
                 static_cast<double>(Akf_masked[i][0] + Akf_masked[i][1]);
        }
        std::cout << "tau=" << i << "  " << k1 << "  " << k2 << "\n";
    }

    for (i = 0; i <= tau_user; i++) {
        delete[] Akf_open[i];
        delete[] Akf_masked[i];
    }
    delete[] Akf_open;
    delete[] Akf_masked;
    delete[] P_user_open;
    delete[] P_user_masked;

    // ========================
    // Задание 4
    // ========================
    std::cout << "\n===== ЗАДАНИЕ 4 =====\n";
    std::cout << "Линейный генератор: x^4+x+1 (вариант 3)\n";
    std::cout << "Введите объем выборки (бит, минимум 64): ";
    std::cin >> counter_lin;
    if (counter_lin < 64) return 1;

    uint8_t R_start = 0x9;
    uint8_t R_period = R_start;
    uint64_t period = 0;

    do {
        feedback_bit_lin = (R_period & 0x1) ^ ((R_period >> 1) & 0x1);
        R_period = (R_period >> 1) | (feedback_bit_lin << 3);
        period++;
    } while (R_period != R_start);

    Rlz_lin = R_start;

    uint64_t PL[5][16] = {0};
    uint64_t Akf63[64][2] = {0};
    uint64_t gamma_lin = 0;

    for (uint64_t t = 0; t < counter_lin; t++) {
        feedback_bit_lin = (Rlz_lin & 0x1) ^ ((Rlz_lin >> 1) & 0x1);
        Rlz_lin = (Rlz_lin >> 1) | (feedback_bit_lin << 3);

        bit = feedback_bit_lin;
        gamma_lin = (gamma_lin << 1) | bit;

        for (int len = 1; len <= 4; len++) {
            if (t >= static_cast<uint64_t>(len - 1)) {
                PL[len][gamma_lin & ((1ULL << len) - 1)]++;
            }
        }

        if (t >= 63) {
            for (j = 0; j < 64; j++) {
                Akf63[j][(gamma_lin & 0x1) ^ ((gamma_lin >> j) & 0x1)]++;
            }
        }
    }

    std::cout << "\nПериод: " << period << "\n";
    std::cout << "\nВероятности комбинаций длиной 1..4:\n";
    for (int len = 1; len <= 4; len++) {
        uint64_t den = counter_lin - (len - 1);
        std::cout << "\nДлина " << len << ":\n";
        for (uint64_t k = 0; k < (1ULL << len); k++) {
            std::cout << std::bitset<4>(k).to_string().substr(4 - len) << " : "
                      << static_cast<double>(PL[len][k]) / static_cast<double>(den) << "\n";
        }
    }

    std::cout << "\nАКФ для tau=0..63:\n";
    for (i = 0; i < 64; i++) {
        double k = 0;
        if (Akf63[i][0] + Akf63[i][1]) {
            k = static_cast<double>(static_cast<long long>(Akf63[i][0]) - static_cast<long long>(Akf63[i][1])) /
                static_cast<double>(Akf63[i][0] + Akf63[i][1]);
        }
        std::cout << "tau=" << i << "  K=" << k << "\n";
    }

    std::cout << "\nГотово: все 4 задания выполнены.\n";
    return 0;
}
