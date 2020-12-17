#include <iostream>
#include <iomanip>

struct Obj
{
    Obj(const char* data)
    : m_data(data)
    {
        std::cout << data << std::endl;
    }

    ~Obj()
    {
        std::cout << "~" << m_data << std::endl;
    }

    const char* m_data;
};

int main()
{
    Obj a("a");
    Obj b("b");
    Obj c("c");
}

