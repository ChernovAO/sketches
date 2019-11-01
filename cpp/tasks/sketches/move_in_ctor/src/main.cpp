#include <iostream>

struct S
{
	S()
	{
		std::cout << "Default" << std::endl;
	}

	S(const S&)
	{
		std::cout << "Copy" << std::endl;
	}

	S(const S&&)
	{
		std::cout << "Move" << std::endl;
	}

	static int i;
};

int S::i = 0;

int main(int argc, char** argv)
{
	S s{ S() };

	return s.i;
}