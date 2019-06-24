#include "fibonacci.h"
#include <vector>

int fibonacci(int n)
{
	if (n == 0)
	{
		return 0;
	}
	if (n == 1)
	{
		return 1;
	}

	int prev = 0;
	int current = 1;
	for (size_t i = 2; i <= n; ++i)
	{
		int tmp = current;
		current += prev;
		prev = tmp;
	}

	return current;
}

int fibonacci_last_number(int n)
{
	if (n == 0)
	{
		return 0;
	}
	if (n == 1)
	{
		return 1;
	}

	int prev = 0;
	int current = 1;
	for (size_t i = 2; i <= n; ++i)
	{
		int tmp = current;
		current += prev;
		current %= 10;
		prev = tmp;
	}

	return current;
}

int fibonacci_rem(int n, int m)
{
	std::vector<int> pizzano_cycle;
	pizzano_cycle.reserve(1024 * 1024);
	std::vector<int> repeat;
	repeat.reserve(1024 * 1024);
	pizzano_cycle[0] = 0;
	pizzano_cycle[1] = 1;

	std::size_t index = 0;
	for (size_t i = 2; i <= n; ++i)
	{
		pizzano_cycle[n] = pizzano_cycle[n - 1] + pizzano_cycle[n - 2];
		if (pizzano_cycle[index] == pizzano_cycle[n])
		{
			repeat.push_back(pizzano_cycle[n]);
		}

		if (repeat.size() > 1 && pizzano_cycle[n] == repeat[0])
		{
			break;
		}
	}


	return ;
}
