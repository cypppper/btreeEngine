#include <iostream>
#include <cstddef>
#include "src/btree_index/index.h"

using namespace std;

struct B{
    char a[100];
};

int main() {
    /* idx */
    auto idx = Index<int, B, std::less<int>>::create();
    for (int i = 0; i < 400; i++) {
        cout << "insert : " << i << endl;
        auto ret = idx->Insert(i, B{});
        cout << "end insert : " << i << endl;
        
    }
    cout << idx->dump_struct() << endl;

    /* internal page */

    // auto raw_page = RawPageMgr::create();
    // auto &page = *reinterpret_cast< InternalPage<int, B, int, std::less<int>> *>(raw_page->data());
    // page.Init();

    // cout << page.dump_struct() << endl;
}