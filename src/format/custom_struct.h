#pragma once

#include <array>
#include "fmt/format.h"

struct TestStructA {
    std::array<char, 800> a;
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


template<>
struct fmt::formatter<TestStructA> {
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin())
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const TestStructA & ts, FormatContext &ctx) const -> decltype(ctx.out())
    {
        fmt::format_to(ctx.out(), "TestStructA({}) \n", ts.dump_struct());
        return ctx.out();
    }
};

