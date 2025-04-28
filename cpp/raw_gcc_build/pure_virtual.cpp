#include <iostream>

struct Base {
  Base() {
    f();
  }

  virtual ~Base() = default;

  virtual void f() = 0;
};

struct Child : public Base {
  void f() override {
    std::cout << "Child" << std::endl;
  }
};

int main(int argc, char** argv) {
  Child c;
  return 0;
}
