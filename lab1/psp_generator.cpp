#include <cstdint>
#include <iostream>
#include <bitset>

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
    std::cout << "Количество бит: ";
    std::cin >> counter;

    Rlz2 = Rlz2<<64;
    Rlz2 = Rlz2|Rlz2_1;

    for (i = 0; i < counter; i++) {
        feedback_bit_1 = (Rlz1&0x1)^((Rlz1>>1)&0x1);
        Rlz1 = (Rlz1>>1)|(feedback_bit_1<<n);

        feedback_bit_2 = Rlz2[0]^Rlz2[36];
        Rlz2 = (Rlz2>>1);
        Rlz2.set(99, feedback_bit_2);

        bit = ((feedback_bit_1)^(feedback_bit_2));
        std::cout << bit;
    }
}