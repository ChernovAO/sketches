#include <iostream>

int main(int argc, char** argv)
{
	float a = 2019.f;
	float b = 1e-5f;

	for (size_t i = 0; i < 10000; ++i)
	{
		a += b;
	}

	std::cout << a << std::endl;
}