#include <optional>
#include <string>
#include <iostream>

int main(int argc, char** argv) {
  std::optional<int> int_opt;
  std::optional<std::string> string_opt;

  std::cout << "int opt: " << sizeof(int_opt) << " string opt: " << sizeof(string_opt) << std::endl;

  return 0;
}
