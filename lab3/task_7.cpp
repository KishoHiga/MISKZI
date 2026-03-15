#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

int main() {
  // Вариант 3 (см. таблицу в методичке).
  uint8_t S[16] = {0x0B, 0x03, 0x05, 0x08, 0x02, 0x0F, 0x0A, 0x0D,
                   0x0E, 0x01, 0x07, 0x04, 0x0C, 0x09, 0x06, 0x00};
  int p = 5;

  cout << "Введите имя файла (то же, что в п.5): ";
  string plaintext;
  cin >> plaintext;

  // Чтобы можно было сравнить с результатом п.5, пишем в другой файл.
  string output = plaintext + ".m_eq";

  ifstream infile(plaintext, ios::binary);
  ofstream outfile(output, ios::binary);

  if (!infile.is_open() || !outfile.is_open()) {
    cerr << "Ошибка открытия файлов!" << endl;
    return 1;
  }

  // Формирование таблицы S* (Sa).
  uint8_t Sa[256] = {0};
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      Sa[i * 16 + j] = (S[i] << 4) | S[j];
    }
  }

  // Эквивалентная таблица Seq.
  uint8_t Seq[256] = {0};
  for (int i = 0; i < 256; i++) {
    Seq[i] = (Sa[i] >> p) | (Sa[i] << (8 - p));
  }

  uint8_t buffer = 0;
  while (infile.read(reinterpret_cast<char*>(&buffer), sizeof(buffer))) {
    buffer = Seq[buffer];
    outfile.write(reinterpret_cast<char*>(&buffer), sizeof(buffer));
  }

  infile.close();
  outfile.close();

  cout << "Преобразование завершено." << endl;
  cout << "Сравнение с результатом п.5 (пример): cmp " << plaintext << ".m " << output << endl;
  return 0;
}
