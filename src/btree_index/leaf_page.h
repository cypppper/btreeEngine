#pragma once

#include <algorithm>
#include <cassert>
#include <cstring>
#include <exception>
#include <format>
#include <string>
#include <utility>
#include <iostream>
#include <sstream>


#include "common.h"
#include "../status/status.h"
#include "btree_page.h"




enum class LeafCase: int {
    SplitPage,
    KeyNotFound,
    OK,
    DidMerge,
    DidBorrow
};

template <typename KeyT, typename ValueT, typename KeyComparatorT>
class LeafPage: public BTreePage {
    static size_t constexpr SLOT_CNT = PAGE_SLOT_CNT_CALC<sizeof(KeyT), sizeof(ValueT)>;
    using SelfT = LeafPage<KeyT, ValueT, KeyComparatorT>;
    using PairT = std::pair<KeyT, ValueT>;
    using InternalT = InternalPage<KeyT, ValueT, int, KeyComparatorT>;
    using LeafSplitInfo = SplitInfo<KeyT, ValueT>;
    struct KeyLowerBoundComparator {
        auto operator()(const KeyT& a, const KeyT& b) -> bool {
            KeyComparatorT cmper{};
            return cmper(a, b) < 0;
        }
    };
    using KeyLowerBoundCmpT = KeyLowerBoundComparator;
    using KeyThreeWayCmpT = KeyComparatorT;
private:
    std::array<KeyT, SLOT_CNT> keys;
    std::array<ValueT, SLOT_CNT> vals;
    void PushBack(PairT elem);
    void PushFront(PairT elem);
    auto PopBack() -> PairT;
    auto PopFront() -> PairT;
    
public: 
    LeafPage() = delete;
    LeafPage(const LeafPage& other) = delete;

    void Init() noexcept;

    auto Insert(const KeyT& key, const ValueT& value, std::shared_ptr<LeafSplitInfo>& split_info) -> StatusOr<LeafCase>;
    auto Update(const KeyT& key, const ValueT& value) -> StatusOr<LeafCase>;
    auto Get(const KeyT& key, ValueT& result) const -> StatusOr<LeafCase>;
    auto Remove(
        const KeyT& key, 
        std::shared_ptr<Page>& parent,
        KeyT& parent_merged_key,
        bool is_root
    ) -> StatusOr<LeafCase>;
    auto dump_struct() const -> std::string;
    auto GetFirstElem() const -> PairT;
    auto DumpNodeGraphviz() const -> std::string;
};


auto IsLeafPage(std::shared_ptr<Page>& ptr) -> bool;

LEAF_TEMPLATE_ARGUMENTS
void LeafPage<KeyT, ValueT, KeyComparatorT>::Init() noexcept {
    static_assert(sizeof(LeafPage<KeyT, ValueT, KeyComparatorT>) <= BTREE_PAGE_SIZE);
    BTreePage::Init(BTreePageType::LEAF_PAGE, (int)SLOT_CNT);
}



LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::Insert(const KeyT& key, const ValueT& value, std::shared_ptr<LeafSplitInfo>& split_info) -> StatusOr<LeafCase> {
    /*
        insert kv into page
        1. find position to insert
        2. memorymove
        3. check whether reaching max size
        4. if max split
    */

    // 1. find pos to insert
    auto const end_ite = std::begin(keys) + GetSize();
    auto const start_ite = std::begin(keys);
    auto const ge_ite = std::lower_bound(start_ite, end_ite, key, KeyLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. memmove
    std::copy_backward(
        ge_ite,
        end_ite,
        end_ite + 1
    );
    std::copy_backward(
        std::begin(this->vals) + new_idx,
        std::begin(this->vals) + GetSize(),
        std::begin(this->vals) + GetSize() + 1
    );
    // insert
    this->keys[new_idx] = key;
    this->vals[new_idx] = value;
    // 3. check whethre reaching max
    ChangeSizeBy(1);
    if (GetSize() < GetMaxSize()) {
        return {LeafCase::OK};
    }


    // 4. SPLIT
    auto new_page = RawPageMgr::create();
    auto& new_leaf_page = *reinterpret_cast<SelfT*>(new_page->data());
    new_leaf_page.Init();

    assert(split_info.get() != nullptr);
    split_info->new_page_id = new_leaf_page.GetPageId();
    auto mid_pos = GetMinSize();
    split_info->mid_elem = std::pair<KeyT, ValueT> {
        this->keys[mid_pos],
        this->vals[mid_pos]
    };

    auto& new_keys = new_leaf_page.keys;
    auto& new_vals = new_leaf_page.vals;
    std::copy(std::begin(this->keys) + mid_pos + 1, std::begin(this->keys) + GetSize(), std::begin(new_keys));
    std::copy(std::begin(this->vals) + mid_pos + 1, std::begin(this->vals) + GetSize(), std::begin(new_vals));
    new_leaf_page.SetSize(GetSize() - GetMinSize() - 1);
    this->SetSize(GetMinSize());
    return {LeafCase::SplitPage};
}



LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::Update(const KeyT& key, const ValueT& value) -> StatusOr<LeafCase> {
    /*
        update kv in page
        1. find position to update
        2. if not exist, return KeyNotFound
        3. change value at this pos
    */

    // 1. find pos to update
    auto end_ite = std::begin(keys) + GetSize();
    auto start_ite = std::begin(keys);
    auto ge_ite = std::lower_bound(start_ite, end_ite, key, KeyLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. if not exist, return KeyNotFound
    if (ge_ite == end_ite) {
        // no key in leaf page >= key
        return {LeafCase::KeyNotFound};
    } else if (KeyThreeWayCmpT{}(*ge_ite, key) != 0) {
        return {LeafCase::KeyNotFound};
    }

    // 3. change value at this pos
    this->vals[new_idx] = value;
    // std::cout << "[update in leaf] new value: " << this->vals[new_idx].dump_struct() << std::endl;
    return {LeafCase::OK};
}

LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::Get(const KeyT& key, ValueT& result) const -> StatusOr<LeafCase> {
    /*
        get value of given key in page
        1. find pos of key
        2. if not exist return KeyNotFound
        3. return value
    */
    // 1. find pos to update
    auto end_ite = std::begin(keys) + GetSize();
    auto start_ite = std::begin(keys);
    auto ge_ite = std::lower_bound(start_ite, end_ite, key, KeyLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. if not exist, return KeyNotFound
    if (ge_ite == end_ite) {
        // no key in leaf page >= key
        return {LeafCase::KeyNotFound};
    } else if (KeyThreeWayCmpT{}(*ge_ite, key) != 0) {
        return {LeafCase::KeyNotFound};
    }
    result = this->vals[new_idx];
    // 3. change value at this pos
    return {LeafCase::OK};
}

LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::Remove(
    const KeyT& key, 
    std::shared_ptr<Page>& parent,
    KeyT& parent_merged_key,
    bool is_root
) -> StatusOr<LeafCase> {
    /*
        remove kv in page
        1. find pos of key
        2. if not exist return KeyNotFound
        3. remove kv in this pos
        4. if less than min_size, merge
        5. return old value
    */

    // 1. find pos of key
    auto end_ite = std::begin(keys) + GetSize();
    auto start_ite = std::begin(keys);
    auto ge_ite = std::lower_bound(start_ite, end_ite, key, KeyLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);
    
    // 2. if not exist return KeyNotFound
    if (ge_ite == end_ite) {
        // no key in leaf page >= key
        return {LeafCase::KeyNotFound};
    } else if (KeyThreeWayCmpT{}(*ge_ite, key) != 0) {
        return {LeafCase::KeyNotFound};
    }

    // 3. remove kv in this pos
    std::copy(
        ge_ite + 1,
        end_ite,
        ge_ite
    );
    std::copy(
        std::begin(this->vals) + new_idx + 1,
        std::begin(this->vals) + GetSize(),
        std::begin(this->vals) + new_idx
    );
    ChangeSizeBy(-1);

    // 4. if less than min_size, merge
    if (!is_root && GetSize() < GetMinSize()) {
        auto& parent_inner = *reinterpret_cast<InternalT*>(parent->data());
        auto my_pid_idx = parent_inner.GetIdxByPid(GetPageId());
        assert(my_pid_idx != -1);
        auto simbling_pid_idx = (my_pid_idx == parent_inner.GetSize() - 1)? my_pid_idx - 1 : my_pid_idx + 1;
        auto replace_or_merge_pair_idx_in_parent = std::max(my_pid_idx, simbling_pid_idx);
        auto replace_or_merge_pair = parent_inner.ElemAt(replace_or_merge_pair_idx_in_parent);
        auto simbling_pid = parent_inner.PidAt(simbling_pid_idx);

        auto simbling_raw_page = RawPageMgr::get_page(simbling_pid);
        auto& simbling_leaf = *reinterpret_cast<SelfT*>(simbling_raw_page->data());

        // std::cout << std::format("my_page id: {}, simb page_id: {}\n", GetPageId(), simbling_pid);
        if (simbling_leaf.GetSize() > GetMinSize()){
            // can borrow

            // if simbling idx > my idx, move parent to last
            // else move parent elem to first
            PairT borrow_elem_simbling{};
            if (my_pid_idx == simbling_pid_idx - 1) {
                PushBack(replace_or_merge_pair);
                borrow_elem_simbling = simbling_leaf.PopFront();
                parent_inner.SetElemPairAt(replace_or_merge_pair_idx_in_parent, borrow_elem_simbling);
            } else {
                assert(my_pid_idx == simbling_pid_idx + 1);
                PushFront(replace_or_merge_pair);
                borrow_elem_simbling = simbling_leaf.PopBack();
                parent_inner.SetElemPairAt(replace_or_merge_pair_idx_in_parent, borrow_elem_simbling);
            }
            return {LeafCase::DidBorrow};
        } else {
            // merge with simbling
            auto merger_idx = std::min(my_pid_idx, simbling_pid_idx);
            SelfT* merger_leaf_ptr, *mergee_leaf_ptr;
            if (merger_idx == my_pid_idx) {
                merger_leaf_ptr = this;
                mergee_leaf_ptr = &simbling_leaf;
            } else {
                merger_leaf_ptr = &simbling_leaf;
                mergee_leaf_ptr = this;
            }
            auto& merger_leaf = *reinterpret_cast<SelfT*>(merger_leaf_ptr);
            auto& mergee_leaf = *reinterpret_cast<SelfT*>(mergee_leaf_ptr);
            // copy parent borrow elem to merger
            merger_leaf.PushBack(replace_or_merge_pair);
            parent_inner.RemoveElemAndPidAt(replace_or_merge_pair_idx_in_parent);
            // copy mergee to merger
            std::copy(
                std::begin(mergee_leaf.keys),
                std::begin(mergee_leaf.keys) + mergee_leaf.GetSize(),
                std::begin(merger_leaf.keys) + merger_leaf.GetSize()
            );
            std::copy(
                std::begin(mergee_leaf.vals),
                std::begin(mergee_leaf.vals) + mergee_leaf.GetSize(),
                std::begin(merger_leaf.vals) + merger_leaf.GetSize()
            );
            ChangeSizeBy(mergee_leaf.GetSize());
            return {LeafCase::DidMerge};
        }
    }

    // 5. return old value
    return {LeafCase::OK};
}

LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::dump_struct() const -> std::string {
    auto ret = std::format("total size: {}\nslot cnt: {}\nkeyT size: {}\nvalT size: {}\nkeys size: {}\nvals size {}\n"
                            "header size: {}\n",
                            sizeof(LeafPage<KeyT, ValueT, KeyComparatorT>), SLOT_CNT,
                            sizeof(KeyT), sizeof(ValueT),
                            sizeof(this->keys), sizeof(this->vals),
                            LEAF_PAGE_HEADER_SIZE
                        )
        + std::format("start_pos: {}, key pos: {}, vals pos:{} end pos: {}\n",
                        (void*)this, (void*)&this->keys[0], (void*)&this->vals[0], (void*)((char*)this + sizeof(*this))
                    );
    for (int i = 0; i < GetSize(); i++) {
        ret += std::format("key: {}, val: {}\n", this->keys[i], this->vals[i].dump_struct());
    }
    return ret;
}


LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::GetFirstElem() const -> PairT {
    return std::make_pair(this->keys[0], this->vals[0]);
}

LEAF_TEMPLATE_ARGUMENTS
void LeafPage<KeyT, ValueT, KeyComparatorT>::PushBack(PairT elem) {
    this->keys[GetSize()] = elem.first;
    this->vals[GetSize()] = elem.second;
    ChangeSizeBy(1);
}

LEAF_TEMPLATE_ARGUMENTS
void LeafPage<KeyT, ValueT, KeyComparatorT>::PushFront(PairT elem) {
    std::copy_backward(
        std::begin(this->keys),
        std::begin(this->keys) + GetSize(),
        std::begin(this->keys) + GetSize() + 1
    );
    std::copy_backward(
        std::begin(this->vals),
        std::begin(this->vals) + GetSize(),
        std::begin(this->vals) + GetSize() + 1
    );
    this->keys[0] = elem.first;
    this->vals[0] = elem.second;
    ChangeSizeBy(1);
}

LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::PopBack() -> PairT {
    ChangeSizeBy(-1);
    return std::make_pair(
        this->keys[GetSize()],
        this->vals[GetSize()]
    );
}

LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::PopFront() -> PairT {
    auto res = std::make_pair(
        this->keys[0],
        this->vals[0]
    );
    std::copy(
        std::begin(this->keys) + 1,
        std::begin(this->keys) + GetSize(),
        std::begin(this->keys) 
    );
    std::copy(
        std::begin(this->vals) + 1,
        std::begin(this->vals) + GetSize(),
        std::begin(this->vals)
    );
    ChangeSizeBy(-1);
    return res;
}


LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::DumpNodeGraphviz() const -> std::string {
    std::stringstream out;
    out << "  node" << GetPageId() << " [label=\"";
    for (int i = 0; i < GetSize(); ++i) {
        if (i > 0) out << "|";
        out << "<f" << i << "> " << this->keys[i];
    }
    out << "\"];\n";
    return out.str();
}
