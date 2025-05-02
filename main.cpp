#include <algorithm>
#include <iostream>
#include <cstddef>
#include <compare>
#include <cassert>
#include "src/btree_index/index.h"
#include "src/format/custom_struct.h"
using namespace std;

int main() {
    cout << "Running Checks On Btree Index...\n";
    auto idx = Index<int, TestStructA, IntThreeWayCmper>::create();
    for (int i = 0; i < 400; i++) {
        auto ret = idx->Insert(i, TestStructA{});
        assert(ret.Ok());
    }

    for (int i = 0; i < 400; i++) {
        auto new_val = TestStructA{};
        new_val.a = {'h', 'a', 'h', 'a', '\0'};
        auto ret = idx->Update(i, new_val);
        assert(ret.Ok());
    }

    for (int i = 0; i < 400; i++) {
        auto ret = idx->Get(i);
        auto result = ret.Unwrap();
        assert(result->a[0] == 'h');
        assert(result->a[1] == 'a');
        assert(result->a[2] == 'h');
        assert(result->a[3] == 'a');
        assert(result->a[4] == '\0');
    }
    // cout << idx->dump_struct() << endl;

    cout << "All Check Passed!" << endl;
}

