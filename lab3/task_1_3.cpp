#include <cstdint>
#include <iomanip>
#include <iostream>

using namespace std;

int main() {
  // Вариант 3 (см. таблицу в методичке).
  uint8_t S[16] = {0x0B, 0x03, 0x05, 0x08, 0x02, 0x0F, 0x0A, 0x0D,
                   0x0E, 0x01, 0x07, 0x04, 0x0C, 0x09, 0x06, 0x00};
  int p = 5;

  // Из задания: a ∈ V64.
  uint64_t a = 0x0123456789ABCDEFULL;

  // 1) Подстановка над 4-битными подблоками.
  uint64_t out1 = 0;
  for (int i = 0; i < 16; i++) {
    out1 = out1 | ((uint64_t)S[(a >> (4 * i)) & 0x0F] << (4 * i));
  }

  // 2) Эквивалентная таблица замены S* (Sa) для двух смежных 4-битных векторов.
  uint8_t Sa[256]{0};
  for (int i = 0; i < 16; i++)
    for (int j = 0; j < 16; j++) {
      Sa[i * 16 + j] = (S[i] << 4) | S[j];
    }

  uint64_t out2 = 0;
  for (int i = 0; i < 8; i++) {
    out2 = out2 | ((uint64_t)Sa[(a >> (8 * i)) & 0xFF] << (8 * i));
  }

  // 3) Перестановка с использованием циклического сдвига вправо на p бит (по байтам).
  uint64_t out3 = 0;
  for (int i = 0; i < 8; i++) {
    uint8_t byte = (a >> (8 * i)) & 0xFF;
    byte = (byte >> p) | (byte << (8 - p));
    out3 = out3 | ((uint64_t)byte << (8 * i));
  }

  cout << hex << uppercase << setfill('0');
  cout << "a = 0x" << setw(16) << a << endl;
  cout << "p = " << dec << p << endl << endl;

  cout << hex << uppercase << setfill('0');
  cout << "1) Подстановка (4 бита): 0x" << setw(16) << out1 << endl;
  cout << "2) Подстановка (S*):      0x" << setw(16) << out2 << endl;
  cout << "   Сравнение (п.1 == п.2): " << (out1 == out2 ? "OK" : "FAIL") << endl;
  cout << "3) Циклический сдвиг:      0x" << setw(16) << out3 << endl;

  cout << "\nТаблица S* (Sa):\n";
  for (int i = 0; i < 256; i++) {
    cout << setw(2) << (int)Sa[i] << " ";
    if ((i + 1) % 16 == 0) cout << endl;
  }

  return 0;
}
