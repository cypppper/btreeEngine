#include <iostream>
#include <cstddef>
#include "btree_page.h"
#include "src/btree_index/inner_page.h"
#include "src/btree_index/leaf_page.h"
#include "src/btree_index/index.h"

using namespace std;

struct B{
    char a[100];
};

int main() {
    // auto vec = std::vector<char>(4096);
    // auto &page = *reinterpret_cast< LeafPage<int, B, std::less<int>> *>(vec.data());
    // page.Init();

    // for (int i = 0; i < 40; i++) {
    //     auto ret = page.Insert(i, B{});
    //     cout << "ok?" << ret.Ok() << endl;
    // }

    // cout << page.dump_struct() << endl;
    
    
    /* idx */

    auto idx = Index<int, B, std::less<int>>::create();
    for (int i = 0; i < 39; i++) {
        auto ret = idx->Insert(i, B{});
        cout << "ok?" << ret.Ok() << endl;
    }
    cout << idx->dump_struct() << endl;

    /* internal page */

    // auto raw_page = RawPageMgr::create();
    // auto &page = *reinterpret_cast< InternalPage<int, B, int, std::less<int>> *>(raw_page->data());
    // page.Init();

    // cout << page.dump_struct() << endl;
}