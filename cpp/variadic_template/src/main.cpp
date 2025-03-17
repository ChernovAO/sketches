#include <iostream>
#include <vector>
#include <string>

// Variadic template function
template<typename... Args>
void processVector(const std::vector<std::string>& vec, Args... args) {
  // Process the vector
  std::cout << "Vector contents: ";
  for (const auto& str : vec) {
    std::cout << str << " ";
  }
  std::cout << std::endl;

  // Process the variadic arguments
  std::cout << "Variadic arguments: ";
  ((std::cout << args << " "), ...);
  std::cout << std::endl;
}

int main(int argc, char** argv) {
  std::vector<std::string> vec = {"Hello", "World", "C++"};

  // Explicitly pass the vector as the first argument
  processVector(vec, 42, 3.14, "Template");

  return 0;
}

