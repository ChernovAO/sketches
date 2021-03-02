#include <iostream>
#include <iomanip>

struct Item
{
public:
    Item()
    : m_index(0)
    , m_value(0)
    {
        std::cout << "Item()" << std::endl;
    }

    Item(const Item& other)
    : m_index(other.m_index)
    , m_value(other.m_value)
    {
        std::cout << "Item(const Item& other)" << std::endl;
    }

    Item(Item&& other) noexcept
        : m_index(std::move(other.m_index))
        , m_value(std::move(other.m_value))
    {
        std::cout << "Item(const Item&& other)" << std::endl;
    }

    Item& operator = (const Item& other)
    {
        std::cout << "Item& operator = (const Item& other)" << std::endl;

        m_index = other.m_index;
        m_value = other.m_value;

        return *this;
    }

    Item& operator = (Item&& other) noexcept
    {
        m_index = std::move(other.m_index);
        m_value = std::move(other.m_value);

        return *this;
    }

public:
    int m_index;
    char m_value;
};

Item globalItem;

void onReset(Item item)
{
    globalItem = std::move(item);
}

void onMsg(Item item)
{
    onReset(std::move(item));
}

int main(int argc, char** argv)
{
    Item item;
    item.m_index = 2;
    item.m_value = 'A';

    onMsg(item);
    return 0;
}