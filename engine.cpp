#include <algorithm>
#include <iostream>
#include <cstddef>
#include <compare>
#include <cassert>
#include "src/btree_index/index.h"
#include "src/format/custom_struct.h"
#include "src/graphviz/graphviz.h"

using namespace std;

int constexpr TEST_NUM = 10;

int main() {
    auto idx = Index<int, TestStructA, IntThreeWayCmper>::create();
    for (int i = 0; i < TEST_NUM; i++) {
        auto ret = idx->Insert(i, TestStructA{});
        assert(ret.Ok());
    }

    for (int i = 0; i < TEST_NUM; i++) {
        auto new_val = TestStructA{};
        new_val.a = {'h', 'a', 'h', 'a', '\0'};
        auto ret = idx->Update(i, new_val);
        assert(ret.Ok());
    }

    for (int i = 0; i < TEST_NUM; i++) {
        auto ret = idx->Get(i);
        auto result = ret.Unwrap();
        cout << fmt::format("{}\n", result.value());
    }
}

