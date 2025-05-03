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
    cout << "\n\nRunning Checks On Btree Index...\n";
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
        assert(result->a[0] == 'h');
        assert(result->a[1] == 'a');
        assert(result->a[2] == 'h');
        assert(result->a[3] == 'a');
        assert(result->a[4] == '\0');
    }

    auto cont0 = idx->DumpGraphviz();
    GenerateDot({"/tmp/dot/dotinit.dot"}, cont0);

    cout << idx->dump_struct() << endl;

    for (int i = 0; i < TEST_NUM; i++) {
        cout << "[remove] " << i << endl;
        auto ret = idx->Remove(i);
        // cout << idx->dump_struct() << endl;
        auto cont = idx->DumpGraphviz();
        GenerateDot(std::format("/tmp/dot/dot_after_{}.dot", i), cont);
    }


    // cout << "All Check Passed!" << endl;
}

