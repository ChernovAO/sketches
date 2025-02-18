#include <iostream>
#include <string>

std::string prepend(std::string &&msg, const std::string &prefix, const std::string &) {
    [[maybe_unused]] const auto pre = [&msg](const std::string &s) { return s + ':' + std::move(msg) ; } ;
    [[maybe_unused]] const auto app = [msg=std::move(msg)](const std::string &s) { return std::move(msg) + ':' + s ; } ;
    return pre(prefix) ;
}

std::string append(std::string &&msg, const std::string &, const std::string &postfix) {
    [[maybe_unused]] const auto pre = [&msg](const std::string &s) { return s + ':' + std::move(msg) ; } ;
    [[maybe_unused]] const auto app = [msg=std::move(msg)](const std::string &s) { return std::move(msg) + ':' + s ; } ;
    return app(postfix) ;
}

int main() {
    std::cout << prepend("message", "prefix", "postfix") << std::endl ;
    std::cout << append("message", "prefix", "postfix") << std::endl ;
    return 0;
}
