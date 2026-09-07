module for_test;

int main() {
  int total = 0;
  for (int index = 0; index < 4; index = index + 1) {
    total = total + index;
  }
  return total;
}
