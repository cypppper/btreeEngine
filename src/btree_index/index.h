#pragma once

#include "common.h"
#include "inner_page.h"
#include "src/status/status.h"
#include "src/btree_index/btree_page.h"
#include "src/btree_index/leaf_page.h"

#include <iostream>
#include <utility>
#include <memory>
#include <cassert>


template<typename KeyT, typename ValueT, typename KeyComparatorT>
class Index {
    using SelfT = Index<KeyT, ValueT, KeyComparatorT>;
    using LeafT = LeafPage<KeyT, ValueT, KeyComparatorT>;
    using InternalT = InternalPage<KeyT, ValueT, int, KeyComparatorT>;
    using LeafSplitInfoT = SplitInfo<KeyT, ValueT>;
    using InternalSplitInfoT = SplitInfo<KeyT, ValueT>;
public:
    Index() = default;
    static auto create() -> std::unique_ptr<SelfT>;
    auto Insert(const KeyT& key, const ValueT& val) -> Status;
    auto Update(const KeyT& key, const ValueT& new_val) -> Status {

    }
    auto get(const KeyT& key) -> StatusOr<ValueT> {}
    auto remove(const KeyT& key) -> StatusOr<ValueT> {}
    auto dump_struct() const -> std::string;
private:
    static auto InsertFromInternal(std::shared_ptr<Page>& cur_page, std::shared_ptr<InternalSplitInfoT>& split_info);
    static auto GetLeaf(std::shared_ptr<Page>& ptr) -> LeafT&;
    static auto GetInner(std::shared_ptr<Page>& ptr) -> InternalT&;
    std::shared_ptr<Page> root;
};

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::GetLeaf(std::shared_ptr<Page>& ptr) -> LeafT& {
    return *reinterpret_cast<LeafT*>(ptr->data());
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::GetInner(std::shared_ptr<Page>& ptr) -> InternalT& {
    return *reinterpret_cast<InternalT*>(ptr->data());
}


INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::Insert(const KeyT& key, const ValueT& val) -> Status {
    if (CheckIsLeafPage(this->root)) {
        auto& leaf_root = GetLeaf(this->root);
        auto leaf_split_info = std::make_shared<LeafSplitInfoT>();
        auto result = leaf_root.Insert(key, val, leaf_split_info);
        if (result.Ok()) {
            std::cout << "Insert Ok \n";
        } else {
            std::cout << std::format("Split when key = {}\n", key);
            // split
            assert(leaf_split_info.get() != nullptr);
            auto new_root_page = RawPageMgr::create();
            auto& inner_new_root = GetInner(new_root_page);
            inner_new_root.Init();
            inner_new_root.SetInitialState(leaf_split_info->mid_elem, leaf_root.GetPageId(), leaf_split_info->new_page_id);
            this->root = new_root_page;
        }
        return {};
    } else {
        // root is internal
        /*
            1. find leaf
            2. insert
            3. do split if happen
        */

    }
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::dump_struct() const -> std::string {
    if (CheckIsLeafPage(this->root)) {
        auto btree_page = reinterpret_cast<LeafT*>(this->root->data());
        return btree_page->dump_struct();
    } else {
        auto btree_page = reinterpret_cast<InternalT*>(this->root->data());
        return btree_page->dump_struct();
    }
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::create() -> std::unique_ptr<SelfT> {
    auto idx = std::make_unique<SelfT>();
    idx->root = RawPageMgr::create();
    auto& root_leaf = GetLeaf(idx->root);
    root_leaf.Init();
    return idx;
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::InsertFromInternal(std::shared_ptr<Page>& cur_page, std::shared_ptr<InternalSplitInfoT>& split_info) {
    
}
