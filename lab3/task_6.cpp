#include <cstdint>
#include <iomanip>
#include <iostream>

using namespace std;

int main() {
  // Вариант 3 (см. таблицу в методичке).
  uint8_t S[16] = {0x0B, 0x03, 0x05, 0x08, 0x02, 0x0F, 0x0A, 0x0D,
                   0x0E, 0x01, 0x07, 0x04, 0x0C, 0x09, 0x06, 0x00};
  int p = 5;

  // Формирование таблицы S* (Sa) (смежные 4-битные подблоки).
  uint8_t Sa[256] = {0};
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      Sa[i * 16 + j] = (S[i] << 4) | S[j];
    }
  }

  // Эквивалентная таблица подстановки Seq для схемы (рис. 16):
  // Seq[x] = (Sa[x] >> p) | (Sa[x] << (8 - p)).
  uint8_t Seq[256] = {0};
  for (int i = 0; i < 256; i++) {
    Seq[i] = (Sa[i] >> p) | (Sa[i] << (8 - p));
  }

  cout << "Эквивалентная таблица подстановки Seq (256 байт):\n";
  cout << hex << uppercase << setfill('0');
  for (int i = 0; i < 256; i++) {
    cout << setw(2) << (int)Seq[i] << " ";
    if ((i + 1) % 16 == 0) cout << "\n";
  }

  return 0;
}
