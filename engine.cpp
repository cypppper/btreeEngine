#include <iostream>
#include <cstddef>
#include <cassert>
#include "src/btree_index/index.h"
#include "src/format/custom_struct.h"
#include "src/graphviz/graphviz.h"
#include "src/factories/op_factory.h"

using namespace std;

int constexpr TEST_NUM = 10;

int main() {
    auto idx = Index<int, TestStructA, IntThreeWayCmper>::create();
    auto factory = IndexOpFactory<int, TestStructA, IntThreeWayCmper>{};
    for (int i = 0; i < TEST_NUM; i++) {
        auto factory_op = factory.make_op<OPTYPE::INSERT>(idx, i, TestStructA{});
        factory_op();
    }

    for (int i = 0; i < TEST_NUM; i++) {
        auto new_val = TestStructA{};
        new_val.a = {'h', 'a', 'h', 'a', '\0'};
        auto factory_op = factory.make_op<OPTYPE::UPDATE>(idx, i, new_val);
        factory_op();
    }

    for (int i = 0; i < TEST_NUM; i++) {
        auto res = std::optional<TestStructA>{};
        auto factory_op = factory.make_op<OPTYPE::GET>(idx, i, res);
        factory_op();
        cout << fmt::format("{}\n", res.value());
    }

    for (int i = 0; i < TEST_NUM; i++) {
        auto factory_op = factory.make_op<OPTYPE::REMOVE>(idx, i);
        factory_op();
    }
}

