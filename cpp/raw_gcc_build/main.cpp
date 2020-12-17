#include <thread>
#include <iostream>
#include <iomanip>

int main(int argc, char** argv)
{
	auto thread_worker = [] (int argc, char** argv)
	{
		for (std::size_t i = 0; i < argc; ++i)
		{
			std::cout << argv[i] << std::endl;
		}
	};

	std::thread worker(thread_worker);
	worker.join();

	return 0;
}

