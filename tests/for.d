module for_test;

int main() {
  int total = 0;
  for (int index = 0; index < 10; index = index + 1) {
    if (index == 3)
      continue;
    if (index == 7)
      break;
    total = total + index;
  }
  return total;
}
