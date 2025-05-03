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
    cout << "\n\n============ START CHECKING BTREE INDEX ==================\n";


    cout << "\n\n-----Running [INSERT] Check On Btree Index...--------\n";
    auto idx = Index<int, TestStructA, IntThreeWayCmper>::create();
    for (int i = 0; i < TEST_NUM; i++) {
        auto ret = idx->Insert(i, TestStructA{});
        assert(ret.Ok());
    }
    cout << "\n\n\t\t [INSERT] Check Passed! \n";

    cout << "\n\n-----Running [UPDATE] Check On Btree Index...--------\n";

    for (int i = 0; i < TEST_NUM; i++) {
        auto new_val = TestStructA{};
        new_val.a = {'h', 'a', 'h', 'a', '\0'};
        auto ret = idx->Update(i, new_val);
        assert(ret.Ok());
    }
    cout << "\n\n\t\t [UPDATE] Check Passed! \n";

    cout << "\n\n-----Running [GET] Check On Btree Index...--------\n";
    for (int i = 0; i < TEST_NUM; i++) {
        auto ret = idx->Get(i);
        auto result = ret.Unwrap();
        assert(result->a[0] == 'h');
        assert(result->a[1] == 'a');
        assert(result->a[2] == 'h');
        assert(result->a[3] == 'a');
        assert(result->a[4] == '\0');
    }
    cout << "\n\n\t\t [GET] Check Passed! \n";

    cout << "\n\n-----DUMP Current Btree Structure Into Graphviz Format...--------\n";
    auto cont0 = idx->DumpGraphviz();
    GenerateDot({"dots/dotinit.dot"}, cont0);
    cout << "\n\n\t\t [DUMP] Finished! \n";


    cout << "\n\n-----Running [DELETE] Check On Btree Index...--------\n";
    for (int i = 0; i < TEST_NUM; i++) {
        // cout << "[remove] " << i << endl;
        auto ret = idx->Remove(i);
        ret.Unwrap();
        // cout << idx->dump_struct() << endl;
        // auto cont = idx->DumpGraphviz();
        // GenerateDot(std::format("dots/dot_after_remove_{}.dot", i), cont);
    }

    for (int i = 0; i < TEST_NUM; i++) {
        auto ret = idx->Get(i);
        auto result = ret.Unwrap();
        assert(!result.has_value());
    }
    cout << "\n\n\t\t [DELETE] Check Passed! \n";

    cout << "\n\n =============CHECK RESULT: All Check Passed!==============\n\n" << endl;
}

