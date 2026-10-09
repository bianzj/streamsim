#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "production_split.hpp"

int main() {
    unsigned checks = 0;
    auto verify = [&](std::string source, std::string delimiters,
                      std::vector<std::string> expected) {
        const auto original = source;
        const auto originalDelimiters = delimiters;
        const auto actual = Utils::splitt(source, delimiters);
        if (actual != expected || source != original || delimiters != originalDelimiters) {
            std::cerr << "Incorrect text tokenization for: " << source << '\n';
            std::exit(1);
        }
        ++checks;
    };
    verify("1", " ", {"1"});
    verify("A", ",", {"A"});
    verify("10", " ", {"10"});
    verify("", " ", {});
    verify("  ", " ", {});
    verify(" ", " ", {});
    verify("1 ", " ", {"1"});
    verify(" 1", " ", {"1"});
    verify(" 1 ", " ", {"1"});
    verify("1 2", " ", {"1", "2"});
    verify("10 2", " ", {"10", "2"});
    verify("  a   b  c ", " ", {"a", "b", "c"});
    verify("0 25 12 1013 2 500 350", " ", {"0", "25", "12", "1013", "2", "500", "350"});
    verify("1\t2\r\n3", " \t\r\n", {"1", "2", "3"});
    verify(" 1,2;;3 ", " ,;", {"1", "2", "3"});
    verify("foo::bar", ":", {"foo", "bar"});
    verify("::foo::bar::", ":", {"foo", "bar"});
    verify("::", ":", {});
    verify("abc", "", {"abc"});
    verify("", "", {});
    verify("-1.5 2e-3 +7", " ", {"-1.5", "2e-3", "+7"});
    verify("C:/inputs/1.txt", " ", {"C:/inputs/1.txt"});
    verify(std::string(100000, ' '), " ", {});
    verify(std::string(100000, 'a'), " ", {std::string(100000, 'a')});
    std::cout << "{\"test\":\"production_text_split\",\"checks\":" << checks << "}\n";
}
