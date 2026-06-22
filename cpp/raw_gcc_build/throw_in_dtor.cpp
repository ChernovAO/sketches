#include <iostream>
#include <stdexcept>
#include <string>

int g_counter = 0;

struct A {
  ~A() noexcept (false)
  {
    throw(std::runtime_error("A." + std::to_string(a)));
  }

  int a = ++g_counter;
};

struct B {
  ~B() noexcept (false) {
    throw(std::runtime_error("B." + std::to_string(b)));
  }

  int b = ++g_counter;
  A a;
};

int main(int argc, char** argv) {
  try {
    std::cout << "case 1" << std::endl;
    A a;
  } catch (const std::exception& e) {
    std::cout << "case 1: " << e.what() << std::endl;
  }

  try {
    std::cout << "case 2" << std::endl;
    B a;
  } catch (const std::exception& e) {
    std::cout << "case 2: " << e.what() << std::endl;
  }
  return 0;
}
