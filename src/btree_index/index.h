#pragma once

#include "common.h"
#include "inner_page.h"
#include "src/status/status.h"
#include "src/btree_index/btree_page.h"
#include "src/btree_index/leaf_page.h"

#include <cstddef>
#include <iostream>
#include <utility>
#include <memory>
#include <cassert>

enum class IndexCase: int {
    Ok,
    ChildInsertPageSplit,
    KeyNotFound,
};

template<typename KeyT, typename ValueT, typename KeyComparatorT>
class Index {
    using SelfT = Index<KeyT, ValueT, KeyComparatorT>;
    using LeafT = LeafPage<KeyT, ValueT, KeyComparatorT>;
    using InternalT = InternalPage<KeyT, ValueT, int, KeyComparatorT>;
    using PidT = int;
    using LeafSplitInfoT = SplitInfo<KeyT, ValueT>;
    using InternalSplitInfoT = SplitInfo<KeyT, ValueT>;
public:
    Index() = default;
    static auto create() -> std::unique_ptr<SelfT>;
    auto Insert(const KeyT& key, const ValueT& val) -> Status;
    auto Update(const KeyT& key, const ValueT& new_val) -> Status;
    auto get(const KeyT& key) -> StatusOr<ValueT> {}
    auto remove(const KeyT& key) -> StatusOr<ValueT> {}
    auto dump_struct() const -> std::string;
private:
    static auto InsertFromInternal(std::shared_ptr<Page>& cur_page, const KeyT& key, 
        const ValueT& value, std::shared_ptr<InternalSplitInfoT>& split_info) -> StatusOr<IndexCase>;
    static auto UpdateFromInternal(std::shared_ptr<Page>& cur_page, const KeyT& key, 
        const ValueT& value) -> StatusOr<IndexCase>;
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
    // std::cout << "root addr: " << (void *)this->root.get() << std::endl;
    if (CheckIsLeafPage(this->root)) {
        auto& leaf_root = GetLeaf(this->root);
        auto leaf_split_info = std::make_shared<LeafSplitInfoT>();
        auto leaf_insert_res = leaf_root.Insert(key, val, leaf_split_info);
        if (leaf_insert_res.Ok()) {
            auto leaf_case = leaf_insert_res.unwrap();
            if (leaf_case == LeafCase::OK) {
                // std::cout << "leaf insert ok\n";
            } else if (leaf_case == LeafCase::SplitPage) {
                // std::cout << std::format("Leaf Split when key = {}\n", key);
                // split
                assert(leaf_split_info.get() != nullptr);
                auto new_root_page = RawPageMgr::create();
                auto& inner_new_root = GetInner(new_root_page);
                inner_new_root.Init();
                // std::cout << "leaf_page?" << inner_new_root.IsLeafPage() << "  |  " << CheckIsLeafPage(new_root_page)<< std::endl;
                inner_new_root.SetInitialState(leaf_split_info->mid_elem, leaf_root.GetPageId(), leaf_split_info->new_page_id);
                this->root = new_root_page;
                // std::cout << "new root addr: " << (void *)this->root.get() << std::endl;
            }
        } else {
            std::cout << "leaf insert error!\n";
            leaf_insert_res.unwrap();
            exit(-1);
        }
        return {};
    } else {
        // root is internal
        /*
            1. find leaf
            2. insert
            3. do split if happen
        */
        auto old_root_split_info = std::make_shared<InternalSplitInfoT>();
        auto internal_insert_res = InsertFromInternal(this->root, key, val, old_root_split_info);
        if (internal_insert_res.Ok()) {
            auto iicase =internal_insert_res.unwrap();
            if (iicase == IndexCase::Ok) {
                // std::cout << "insert ok in internal insert\n";
                return {};
            } else if (iicase == IndexCase::ChildInsertPageSplit) {
                // std::cout << std::format("internal split at key: {}", old_root_split_info->mid_elem.first);
                assert(old_root_split_info.get() != nullptr);
                auto new_root_page = RawPageMgr::create();
                auto& inner_new_root = GetInner(new_root_page);
                inner_new_root.Init();
                auto& inner_old_root = GetInner(this->root);
                auto old_root_pid = inner_old_root.GetPageId();
                inner_new_root.SetInitialState(old_root_split_info->mid_elem, old_root_pid, old_root_split_info->new_page_id);
                this->root = new_root_page;
                // std::cout << "new root addr: " << (void *)this->root.get() << std::endl;
            }
        }
        return {};
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
auto Index<KeyT, ValueT, KeyComparatorT>::InsertFromInternal(std::shared_ptr<Page>& cur_page, const KeyT& key, 
    const ValueT& value, std::shared_ptr<InternalSplitInfoT>& cur_split_info) -> StatusOr<IndexCase> {
    if (CheckIsLeafPage(cur_page)) {
        // insert in leaf
        auto& cur_leaf = GetLeaf(cur_page);
        auto leaf_split_info = std::make_shared<LeafSplitInfoT>();
        auto leaf_insert_res = cur_leaf.Insert(key, value, leaf_split_info);
        auto leaf_case = leaf_insert_res.unwrap();
        if (leaf_case == LeafCase::OK) {
            return {IndexCase::Ok};
        } else if (leaf_case == LeafCase::SplitPage) {
            assert(leaf_split_info.get() != nullptr);
            auto elem = leaf_split_info->mid_elem; // pair<key, value>
            auto new_pid = leaf_split_info->new_page_id;
            // insert mid key, mid value, new pid into cur page
            // TODO
            cur_split_info->mid_elem = elem;
            cur_split_info->new_page_id = new_pid;
            return {IndexCase::ChildInsertPageSplit};
        }
    } else {
        // internal page
        auto& cur_inner = GetInner(cur_page);
        auto get_pid_res = cur_inner.GetChildPid(key);
        auto child_pid = get_pid_res.unwrap();
        auto child_page = RawPageMgr::get_page(child_pid);
        auto child_insert_res = InsertFromInternal(child_page, key, value, cur_split_info);
        auto child_insert_case = child_insert_res.unwrap();
        if (child_insert_case == IndexCase::Ok) {
            return {IndexCase::Ok};
        } else if (child_insert_case == IndexCase::ChildInsertPageSplit) {
            auto cur_insert_elem = cur_split_info->mid_elem; // pair<key, value>
            auto cur_insert_pid = cur_split_info->new_page_id;
            auto child_split_info = std::make_shared<InternalSplitInfoT>();
            auto inner_insert_res = cur_inner.Insert(cur_insert_elem.first, cur_insert_elem.second, cur_insert_pid, child_split_info);
            auto inner_insert_case = inner_insert_res.unwrap();
            if (inner_insert_case == InternalCase::OK) {
                return {IndexCase::Ok};
            } else {
                assert(child_split_info.get() != nullptr);
                auto elem = child_split_info->mid_elem; // pair<key, value>
                auto new_pid = child_split_info->new_page_id;
                // insert mid key, mid value, new pid into cur page
                // TODO
                cur_split_info->mid_elem = elem;
                cur_split_info->new_page_id = new_pid;
                return {IndexCase::ChildInsertPageSplit};
            }
        }
    }
    return {IndexCase::Ok};
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::Update(const KeyT& key, const ValueT& new_val) -> Status {
    std::cout << "root addr: " << (void *)this->root.get() << std::endl;
    if (CheckIsLeafPage(this->root)) {
        auto& leaf_root = GetLeaf(this->root);
        auto leaf_split_info = std::make_shared<LeafSplitInfoT>();
        auto leaf_insert_res = leaf_root.Update(key, new_val);
        if (leaf_insert_res.Ok()) {
            auto leaf_case = leaf_insert_res.unwrap();
            if (leaf_case == LeafCase::OK) {
                std::cout << "root is leaf! update ok\n";
            } else if (leaf_case == LeafCase::KeyNotFound) {
                std::cout << std::format("root is leaf! KeyNotFound\n");
            } else {
                std::cout << "should not reach here\n";
                assert(false);
            }
        } else {
            std::cout << "root is leaf! update error!\n";
            leaf_insert_res.unwrap();
            assert(false);
        }
        return {};
    } else {
        // root is internal
        /*
            1. find leaf
            2. insert
            3. do split if happen
        */
        auto old_root_split_info = std::make_shared<InternalSplitInfoT>();
        auto internal_update_res = UpdateFromInternal(this->root, key, new_val);
        if (internal_update_res.Ok()) {
            auto iucase =internal_update_res.unwrap();
            if (iucase == IndexCase::Ok) {
                std::cout << "root is internal! update ok\n";
            } else if (iucase == IndexCase::KeyNotFound) {
                std::cout << std::format("root is internal! KeyNotFound\n");
            } else {
                std::cout << "should not reach here\n";
                assert(false);
            }
        } else {
            std::cout << "root is internal! update error!\n";
            internal_update_res.unwrap();
            assert(false);
        }
        return {};
    }
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::UpdateFromInternal(std::shared_ptr<Page>& cur_page, const KeyT& key, 
                                                                 const ValueT& new_val) -> StatusOr<IndexCase> {
    if (CheckIsLeafPage(cur_page)) {
        // Update in leaf
        auto& cur_leaf = GetLeaf(cur_page);
        auto leaf_split_info = std::make_shared<LeafSplitInfoT>();
        auto leaf_insert_res = cur_leaf.Update(key, new_val);
        auto leaf_case = leaf_insert_res.unwrap();
        if (leaf_case == LeafCase::OK) {
            return {IndexCase::Ok};
        } else if (leaf_case == LeafCase::KeyNotFound) {
            return {IndexCase::KeyNotFound};
        } else {
            std::cout << "should not reach here!\n";
            assert(false);
        }
    } else {
        // internal page
        auto& cur_inner = GetInner(cur_page);
        PidT get_pid{};
        auto cur_inner_get_res = cur_inner.DoUpdateOrGetChild(key, new_val, get_pid);
        auto cur_update_get_case = cur_inner_get_res.unwrap();
        if (cur_update_get_case == InternalCase::OK) {
            return {IndexCase::Ok};
        } else if (cur_update_get_case == InternalCase::GetChildPageId) {
            auto child_pid = get_pid;
            auto child_page = RawPageMgr::get_page(child_pid);
            auto child_insert_res = UpdateFromInternal(child_page, key, new_val);
            auto child_insert_case = child_insert_res.unwrap();
            if (child_insert_case == IndexCase::Ok || child_insert_case == IndexCase::KeyNotFound) {
                return {child_insert_case};
            } else {
                std::cout << "should not reach here!\n";
                assert(false);
            }
        } else {
            std::cout << "should not reach here!\n";
            assert(false);
        }
        
    }
    return {IndexCase::Ok};
}
