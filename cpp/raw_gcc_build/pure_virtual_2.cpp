#include <iostream>
class A {
public:
    virtual void f()=0;

    A() {
      g();
    }

    void g(){f();}
};

class B : public A {
public:
    void f() override {
        std::cout << "B::f()";
        }
    B() {
        f();
    }
};

int main() {
    B b;
    return 0;
}

