#include <cstdint>
#include <iomanip>
#include <iostream>

using namespace std;

uint8_t komb(uint8_t x, const uint8_t S[16], int p) {
  /*Старшие 4 бита*/
  uint8_t x1 = x >> 4 & 0x0F;
  /*Младшие 4 бита*/
  uint8_t x2 = x & 0x0F;

  /*Подстановка*/
  uint8_t y1 = S[x1];
  uint8_t y2 = S[x2];

  /*Объединение*/
  uint8_t a = (y1 << 4) | y2;

  /*циклический сдвиг*/
  uint8_t b = (a >> p) | (a << (8 - p));

  return b;
}

int main() {
  // Вариант 3 (см. таблицу в методичке).
  uint8_t S[16] = {0x0B, 0x03, 0x05, 0x08, 0x02, 0x0F, 0x0A, 0x0D,
                   0x0E, 0x01, 0x07, 0x04, 0x0C, 0x09, 0x06, 0x00};
  int p = 5;

  // Для проверки можно ввести любой байт x.
  cout << "Введите x (00..FF) в hex: ";
  unsigned x_in = 0;
  if (!(cin >> hex >> x_in) || x_in > 0xFFU) {
    cerr << "Некорректный ввод." << endl;
    return 1;
  }

  uint8_t x = (uint8_t)(x_in & 0xFFU);
  uint8_t b = komb(x, S, p);

  cout << hex << uppercase << setfill('0');
  cout << "b = 0x" << setw(2) << (int)b << endl;
  return 0;
}
