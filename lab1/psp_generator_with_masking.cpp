#include <cstdint>
#include <iostream>
#include <bitset>
#include <fstream>
#include <string>

uint64_t bit;

//K1(x)=x^12+x+1
int n = 12;
int i;
int counter;

uint16_t Rlz1 = 0xD4B; //0000110101001011
uint16_t feedback_bit_1;

//K2(x)=x^100+x^37+1 точки съема 100 и 63
uint64_t feedback_bit_2;

std::bitset<100> Rlz2(0xABFCBBAEA); // старшие биты
std::bitset<100> Rlz2_1(0xAAFEBBECFEBAAFCA); //младшие биты

int main() {
    std::string input_file_name;
    std::string output_file_name;

    std::cout << "Введите имя входного файла: ";
    std::getline(std::cin >> std::ws, input_file_name);

    std::cout << "Введите имя выходного файла: ";
    std::getline(std::cin >> std::ws, output_file_name);

    std::ifstream input_file(input_file_name, std::ios::binary);
    if (!input_file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть входной файл\n";
        return 1;
    }

    std::ofstream output_file(output_file_name, std::ios::binary);
    if (!output_file.is_open()) {
        std::cerr << "Ошибка: не удалось создать выходной файл\n";
        return 1;
    }

    Rlz2 = Rlz2 << 64;
    Rlz2 = Rlz2 | Rlz2_1;

    char input_byte;
    uint8_t gamma_byte;

    while (input_file.get(input_byte)) {
        gamma_byte = 0;

        for (i = 0; i < 8; i++) {
            feedback_bit_1 = (Rlz1 & 0x1) ^ ((Rlz1 >> 1) & 0x1);
            Rlz1 = (Rlz1 >> 1) | (feedback_bit_1 << n);

            feedback_bit_2 = Rlz2[0] ^ Rlz2[36];
            Rlz2 = (Rlz2 >> 1);
            Rlz2.set(99, feedback_bit_2);

            bit = (feedback_bit_1) ^ (feedback_bit_2);
            gamma_byte = gamma_byte | (static_cast<uint8_t>(bit) << i);
        }

        output_file.put(static_cast<char>(static_cast<uint8_t>(input_byte) ^ gamma_byte));
    }

    if (!input_file.eof()) {
        std::cerr << "Ошибка чтения входного файла\n";
        return 1;
    }

    std::cout << "Маскирование завершено. Результат записан в файл: " << output_file_name << "\n";

    return 0;
}
