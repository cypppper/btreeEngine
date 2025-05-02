#include <__compare/compare_three_way.h>
#include <algorithm>
#include <iostream>
#include <cstddef>
#include <compare>
#include "src/btree_index/index.h"
#include "src/format/custom_struct.h"
using namespace std;



int main() {
    /* idx */
    auto idx = Index<int, TestStructA, IntThreeWayCmper>::create();
    for (int i = 0; i < 400; i++) {
        // cout << "insert : " << i << endl;
        auto ret = idx->Insert(i, TestStructA{});
        // cout << "end insert : " << i << endl;
    }

    for (int i = 0; i < 400; i++) {
        // cout << "update : " << i << endl;
        auto new_val = TestStructA{};
        new_val.a = {'h', 'a', 'h', 'a', '\0'};
        auto ret = idx->Update(i, new_val);
        // cout << "end update : " << i << endl;
    }

    cout << idx->dump_struct() << endl;

    /* internal page */

    // auto raw_page = RawPageMgr::create();
    // auto &page = *reinterpret_cast< InternalPage<int, B, int, std::less<int>> *>(raw_page->data());
    // page.Init();

    // cout << page.dump_struct() << endl;
}