#pragma once

#include <array>
#include <format>

struct TestStructA {
    std::array<char, 300> a;
    TestStructA& operator=(const TestStructA& other) {
        std::copy(std::begin(other.a), std::end(other.a), std::begin(this->a));
        return *this;
    };

    auto dump_struct() const -> std::string {
        return std::string(this->a.begin());
    }
};

struct IntThreeWayCmper {
    auto operator()(const int& a, const int& b) -> int{
        return (a > b) - (a < b);
    }
};


