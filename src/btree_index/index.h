#pragma once

#include "common.h"
#include "inner_page.h"
#include "../status/status.h"
#include "btree_page.h"
#include "leaf_page.h"

#include <cstddef>
#include <iostream>
#include <optional>
#include <sstream>
#include <utility>
#include <memory>
#include <cassert>
#include <variant>

enum class IndexCase: int {
    Ok,
    ChildInsertPageSplit,
    KeyNotFound,
    ChildRemoveDidMerge,
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
    auto Get(const KeyT& key) -> StatusOr<std::optional<ValueT>>;
    auto Remove(const KeyT& key) -> Status;
    auto dump_struct() const -> std::string;

    auto DumpGraphviz() -> std::string;

private:
    static auto InsertFromInternal(std::shared_ptr<Page>& cur_page, const KeyT& key, 
        const ValueT& value, std::shared_ptr<InternalSplitInfoT>& split_info) -> StatusOr<IndexCase>;
    static auto UpdateFromInternal(std::shared_ptr<Page>& cur_page, const KeyT& key, 
        const ValueT& value) -> StatusOr<IndexCase>;
    static auto GetFromInternal(std::shared_ptr<Page>& cur_page, const KeyT& key, ValueT& result) -> StatusOr<IndexCase>;
    static auto RemoveFromInternal(std::shared_ptr<Page>& cur_page, 
        std::shared_ptr<Page> parent_page, 
        const KeyT& key,
        KeyT& parent_merged_key
    ) -> StatusOr<IndexCase>;
    static auto GetLeaf(std::shared_ptr<Page>& ptr) -> LeafT&;
    static auto GetInner(std::shared_ptr<Page>& ptr) -> InternalT&;
    static auto DoBorrowOrMerge(std::shared_ptr<Page>& me, std::shared_ptr<Page>& parent, KeyT& del_key_in_parent) -> StatusOr<IndexCase>;
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
            auto leaf_case = leaf_insert_res.Unwrap();
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
            leaf_insert_res.Unwrap();
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
            auto iicase =internal_insert_res.Unwrap();
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
        auto leaf_case = leaf_insert_res.Unwrap();
        if (leaf_case == LeafCase::OK) {
            return {IndexCase::Ok};
        } else if (leaf_case == LeafCase::SplitPage) {
            assert(leaf_split_info.get() != nullptr);
            auto elem = leaf_split_info->mid_elem; // pair<key, value>
            auto new_pid = leaf_split_info->new_page_id;
            // insert mid key, mid value, new pid into cur page
            cur_split_info->mid_elem = elem;
            cur_split_info->new_page_id = new_pid;
            return {IndexCase::ChildInsertPageSplit};
        }
    } else {
        // internal page
        auto& cur_inner = GetInner(cur_page);
        std::variant<ValueT, PidT> get_result;
        auto get_pid_res = cur_inner.GetChildPidOrValue(key, get_result);
        auto get_pid_case = get_pid_res.Unwrap();
        if (get_pid_case == InternalCase::GetValue) {
            // key is duplicate
            return {make_exception<KeyDuplicateException>()};
        } else if (get_pid_case != InternalCase::GetChildPageId) {
            std::cout << "should not reach here!\n";
            exit(-1);
        }
        auto child_pid = std::get<PidT>(get_result);
        auto child_page = RawPageMgr::get_page(child_pid);
        auto child_insert_res = InsertFromInternal(child_page, key, value, cur_split_info);
        auto child_insert_case = child_insert_res.Unwrap();
        if (child_insert_case == IndexCase::Ok) {
            return {IndexCase::Ok};
        } else if (child_insert_case == IndexCase::ChildInsertPageSplit) {
            auto cur_insert_elem = cur_split_info->mid_elem; // pair<key, value>
            auto cur_insert_pid = cur_split_info->new_page_id;
            auto child_split_info = std::make_shared<InternalSplitInfoT>();
            auto inner_insert_res = cur_inner.Insert(cur_insert_elem.first, cur_insert_elem.second, cur_insert_pid, child_split_info);
            auto inner_insert_case = inner_insert_res.Unwrap();
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
    // std::cout << "root addr: " << (void *)this->root.get() << std::endl;
    if (CheckIsLeafPage(this->root)) {
        auto& leaf_root = GetLeaf(this->root);
        auto leaf_split_info = std::make_shared<LeafSplitInfoT>();
        auto leaf_insert_res = leaf_root.Update(key, new_val);
        if (leaf_insert_res.Ok()) {
            auto leaf_case = leaf_insert_res.Unwrap();
            if (leaf_case == LeafCase::OK) {
                std::cout << "root is leaf! update ok\n";
            } else if (leaf_case == LeafCase::KeyNotFound) {
                std::cout << std::format("root is leaf! KeyNotFound\n");
            } else {
                std::cout << "should not reach here\n";
                exit(-1);
            }
        } else {
            std::cout << "root is leaf! update error!\n";
            leaf_insert_res.Unwrap();
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
        auto internal_update_res = UpdateFromInternal(this->root, key, new_val);
        if (internal_update_res.Ok()) {
            auto iucase =internal_update_res.Unwrap();
            if (iucase == IndexCase::Ok) {
                // std::cout << "root is internal! update ok\n";
            } else if (iucase == IndexCase::KeyNotFound) {
                // std::cout << std::format("root is internal! KeyNotFound\n");
            } else {
                std::cout << "should not reach here\n";
                exit(-1);
            }
        } else {
            std::cout << "root is internal! update error!\n";
            internal_update_res.Unwrap();
            exit(-1);
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
        auto leaf_case = leaf_insert_res.Unwrap();
        if (leaf_case == LeafCase::OK) {
            return {IndexCase::Ok};
        } else if (leaf_case == LeafCase::KeyNotFound) {
            return {IndexCase::KeyNotFound};
        } else {
            std::cout << "should not reach here!\n";
            exit(-1);
        }
    } else {
        // internal page
        auto& cur_inner = GetInner(cur_page);
        PidT get_pid{};
        auto cur_inner_get_res = cur_inner.DoUpdateOrGetChild(key, new_val, get_pid);
        auto cur_update_get_case = cur_inner_get_res.Unwrap();
        if (cur_update_get_case == InternalCase::OK) {
            return {IndexCase::Ok};
        } else if (cur_update_get_case == InternalCase::GetChildPageId) {
            auto child_pid = get_pid;
            auto child_page = RawPageMgr::get_page(child_pid);
            auto child_insert_res = UpdateFromInternal(child_page, key, new_val);
            auto child_insert_case = child_insert_res.Unwrap();
            if (child_insert_case == IndexCase::Ok || child_insert_case == IndexCase::KeyNotFound) {
                return {child_insert_case};
            } else {
                std::cout << "should not reach here!\n";
                exit(-1);
            }
        } else {
            std::cout << "should not reach here!\n";
            exit(-1);
        }
        
    }
    return {IndexCase::Ok};
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::Get(const KeyT& key) -> StatusOr<std::optional<ValueT>> {
    if (CheckIsLeafPage(this->root)) {
        auto& leaf_root = GetLeaf(this->root);
        ValueT value{};
        auto leaf_get_res = leaf_root.Get(key, value);
        if (leaf_get_res.Ok()) {
            auto leaf_case = leaf_get_res.Unwrap();
            if (leaf_case == LeafCase::OK) {
                // std::cout << "leaf get ok\n";
                return {std::make_optional(value)};
            } else if (leaf_case == LeafCase::KeyNotFound) {
                // std::cout << std::format("Leaf key not found");
                return {std::optional<ValueT>{}};
            } else {
                std::cout << "should not reach here!\n";
                exit(-1);
            }
        } else {
            std::cout << "leaf insert error!\n";
            leaf_get_res.Unwrap();
            exit(-1);
        }
        return {std::optional<ValueT>{}};
    } else {
        // root is internal
        /*
            1. find leaf
            2. insert
            3. do split if happen
        */
        ValueT result{};
        auto internal_get_res = GetFromInternal(this->root, key, result);
        auto iicase =internal_get_res.Unwrap();
        if (iicase == IndexCase::Ok) {
            // std::cout << "insert ok in internal insert\n";
            return {std::make_optional<ValueT>(result)};
        } else if (iicase == IndexCase::KeyNotFound) {
            return {std::optional<ValueT>{}};
        }
        std::cout << "should not reach here!\n";
        exit(-1);
        return {std::optional<ValueT>{}};
    }
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::GetFromInternal(std::shared_ptr<Page>& cur_page, const KeyT& key, ValueT& result) -> StatusOr<IndexCase> {
    if (CheckIsLeafPage(cur_page)) {
        // insert in leaf
        auto& cur_leaf = GetLeaf(cur_page);
        ValueT val{};
        auto leaf_get_res = cur_leaf.Get(key, val);
        auto leaf_case = leaf_get_res.Unwrap();
        if (leaf_case == LeafCase::OK) {
            result = val;
            return {IndexCase::Ok};
        } else if (leaf_case == LeafCase::KeyNotFound) {
            return {IndexCase::KeyNotFound};
        }
    } else {
        // internal page
        auto& cur_inner = GetInner(cur_page);
        std::variant<ValueT, PidT> get_res;
        auto cur_get_res = cur_inner.GetChildPidOrValue(key, get_res);
        auto cur_get_case = cur_get_res.Unwrap();
        if (cur_get_case == InternalCase::GetValue) {
            result = std::get<ValueT>(get_res);
            return {IndexCase::Ok};
        } else if (cur_get_case == InternalCase::GetChildPageId) {
            auto child_pid = std::get<PidT>(get_res);
            auto child_page = RawPageMgr::get_page(child_pid);

            auto child_get_res = GetFromInternal(child_page, key, result);
            auto child_insert_case = child_get_res.Unwrap();
            if (child_insert_case == IndexCase::Ok || child_insert_case == IndexCase::KeyNotFound) {
                return {child_insert_case};
            } else {
                std::cout << "should not reach here!\n";
                exit(-1);
            }
        } else {
            std::cout << "should not reach here!\n";
            exit(-1);
        }
    }
    return {IndexCase::Ok};
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::Remove(const KeyT& key) -> Status {
    if (CheckIsLeafPage(this->root)) {
        auto& leaf_root = GetLeaf(this->root);
        auto fake_parent = std::shared_ptr<Page>{};
        KeyT fake_key{};
        auto leaf_get_res = leaf_root.Remove(key, fake_parent, fake_key, true);
        if (leaf_get_res.Ok()) {
            auto leaf_case = leaf_get_res.Unwrap();
            if (leaf_case == LeafCase::OK) {
                // std::cout << "root is leaf! remove ok\n";
                return {};
            } else if (leaf_case == LeafCase::KeyNotFound) {
                // std::cout << "root is leaf! key not found\n";
                return {};
            } else {
                std::cout << "should not reach here!\n";
                exit(-1);
            }
        }
        std::cout << "leaf remove error!\n";
        leaf_get_res.Unwrap();
        exit(-1);
    } else {
        // root is internal
        /*
            1. find leaf
            2. insert
            3. do split if happen
        */
        auto& inner_root = GetInner(this->root);
        KeyT child_removed_key{};
        PidT child_pid{};
        auto get_pid_res = inner_root.DowncastRemoveOrGetChildPid(key, child_removed_key, child_pid);
        auto get_pid_case = get_pid_res.Unwrap();

        std::shared_ptr<Page> child_raw_page;
        KeyT cur_merged_key{};
        KeyT new_removed_key{};


        if (get_pid_case == InternalCase::RemoveFound) {
            // downcast effect
            child_raw_page = RawPageMgr::get_page(child_pid);
            new_removed_key = child_removed_key;
        } else if (get_pid_case == InternalCase::GetChildPageId) {
            // get child pid
            child_raw_page = RawPageMgr::get_page(child_pid);
            new_removed_key = key;
        } else {
            std::cout << "should not reach here!\n";
            exit(-1);
        } 
        

        auto internal_remove_result = RemoveFromInternal(child_raw_page, this->root,
            new_removed_key, cur_merged_key);
        auto internal_remove_case = internal_remove_result.Unwrap();
        if (internal_remove_case == IndexCase::Ok) {
            // remove does not upcast effect
            // std::cout << "root is inner! remove ok! \n";
            return {};
        } else if (internal_remove_case == IndexCase::ChildRemoveDidMerge) {
            // check merge
            if (inner_root.GetSize() == 1) {
                // root is empty, change root.
                // std::cout << "root is inner! change to new root!\n";
                this->root = child_raw_page;
            }
            return {};
        } else if (internal_remove_case == IndexCase::KeyNotFound) {
            // std::cout << "root is inner! key not found\n";
            return {};
        }
        std::cout << "should not reach here!\n";
        exit(-1);
    }
}

INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::RemoveFromInternal(
    std::shared_ptr<Page>& cur_page, 
    std::shared_ptr<Page> parent_page, 
    const KeyT& removed_key,
    KeyT& parent_merged_key
) -> StatusOr<IndexCase> {
    auto cur_page_id = reinterpret_cast<BTreePage*>(cur_page->data())->GetPageId();
    
    if (CheckIsLeafPage(cur_page)) {
        // std::cout << "[RemoveFromInternal] (LEAF) on page id : " << cur_page_id << std::endl;
        auto& parent_inner = GetInner(parent_page);
        auto cur_idx_in_parent = parent_inner.GetIdxByPid(cur_page_id);
        assert(cur_idx_in_parent != -1);
        PidT simbling_pid{};
        if (cur_idx_in_parent == parent_inner.GetSize() - 1) {
            simbling_pid = parent_inner.PidAt(cur_idx_in_parent - 1);
        } else {
            simbling_pid = parent_inner.PidAt(cur_idx_in_parent + 1);
        }
        auto simbling_page = RawPageMgr::get_page(simbling_pid);
        // delete in leaf
        auto& cur_leaf = GetLeaf(cur_page);
        auto leaf_remove_res = cur_leaf.Remove(removed_key, parent_page, parent_merged_key, false);
        auto leaf_case = leaf_remove_res.Unwrap();
        if (leaf_case == LeafCase::OK) {
            return {IndexCase::Ok};
        } else if (leaf_case == LeafCase::DidMerge) {
            return {IndexCase::ChildRemoveDidMerge};
        } else if (leaf_case == LeafCase::DidBorrow) {
            return {IndexCase::Ok};
        } else if (leaf_case == LeafCase::KeyNotFound) {
            return {IndexCase::KeyNotFound};
        }
        std::cout << "should not reach here!\n";
        exit(-1);
    } else {
        // std::cout << "[RemoveFromInternal] (INNER) on page id : " << cur_page_id << std::endl;
        // internal page
        auto& cur_inner = GetInner(cur_page);
        KeyT child_removed_key{};
        PidT child_pid{};
        auto get_pid_res = cur_inner.DowncastRemoveOrGetChildPid(removed_key, child_removed_key, child_pid);
        auto get_pid_case = get_pid_res.Unwrap();
        std::shared_ptr<Page> child_raw_page{};
        KeyT cur_merged_key{};
        KeyT new_removed_key{};
        if (get_pid_case == InternalCase::RemoveFound) {
            // downcast effect
            child_raw_page = RawPageMgr::get_page(child_pid);
            new_removed_key = child_removed_key;
        } else if (get_pid_case == InternalCase::GetChildPageId) {
            // get child pid
            child_raw_page = RawPageMgr::get_page(child_pid);
            new_removed_key = removed_key;
        } else {
            std::cout << "should not reach here!\n";
            exit(-1);
        }
        auto internal_remove_result = RemoveFromInternal(child_raw_page, cur_page, new_removed_key, cur_merged_key);
        auto internal_remove_case = internal_remove_result.Unwrap();
        if (internal_remove_case == IndexCase::Ok) {
            // remove does not upcast effect
            return {IndexCase::Ok};
        } else if (internal_remove_case == IndexCase::ChildRemoveDidMerge) {
            // check merge
            auto check_after_remove_res = cur_inner.CheckOrBorrowOrMerge(parent_page);
            auto check_after_remove_case = check_after_remove_res.Unwrap();
            if (check_after_remove_case == InternalCase::RemoveDidMerge) {
                // did merge in cur page
                return {IndexCase::ChildRemoveDidMerge};
            }
            // cur size is safe or did borrow
            return {IndexCase::Ok};
        }
        std::cout << "should not reach here!\n";
        exit(-1);
    }
    return {IndexCase::Ok};
}


INDEX_TEMPLATE_ARGUMENTS
auto Index<KeyT, ValueT, KeyComparatorT>::DumpGraphviz() -> std::string {
    std::stringstream out;
    out << "digraph BTree {\n";
    out << "  node [shape=record];\n";

    if (CheckIsLeafPage(this->root)) {
        auto& leaf = GetLeaf(this->root);
        out << leaf.DumpNodeGraphviz();
    } else {
        auto& inner = GetInner(this->root);
        out << inner.DumpNodeGraphviz();
    }

    out << "}\n";
    return out.str();
}
