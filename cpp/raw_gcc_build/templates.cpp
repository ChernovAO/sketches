#include <iostream>

template<typename T>
class Base {
public:
  Base(T t) : t_(t) {};
protected:
  T t_;
};

template<typename T>
class Child : public Base<T> {
public:
  Child() : Base<T>(T{}) {}

  T t() { return Base<T>::t_; }
};

int main(int argc, char** argv) {
  Child<int> ch;
  std::cout << ch.t() << std::endl;
  return 0;
}
