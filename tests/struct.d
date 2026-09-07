module struct_test;

struct Pair {
  int left;
  int right;
}

int main() {
  Pair pair;
  pair.left = 7;
  pair.right = 5;
  return pair.left + pair.right;
}
