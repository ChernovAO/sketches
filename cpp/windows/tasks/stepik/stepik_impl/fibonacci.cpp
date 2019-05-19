#include "stdafx.h"
#include "fibonacci.h"

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