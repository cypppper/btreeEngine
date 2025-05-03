#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <iostream>
#include <cassert>

#include "common.h"
#include "btree_page.h"
#include "../status/status.h"
#include "index.h"


enum class InternalCase: int {
    KeyFound,
    GetChildPageId,
    GetValue,
    InsertSplit,
    RemoveMatchPidNotFound,
    RemoveFound,
    RemoveDidMerge,
    OK,
};

INTERNAL_TEMPLATE_ARGUMENTS
class InternalPage: public BTreePage {
    static size_t constexpr SLOT_CNT = PAGE_SLOT_CNT_CALC<sizeof(KeyT) + sizeof(ValueT), sizeof(PidT)>;
    using LeafT = LeafPage<KeyT, ValueT, KeyComparatorT>;
    using SelfT = InternalPage<KeyT, ValueT, PidT, KeyComparatorT>;
    using PairT = std::pair<KeyT, ValueT>;
    using KVPidT = std::tuple<KeyT, ValueT, PidT>;
    using InternalSplitInfoT = SplitInfo<KeyT, ValueT>;


    struct PairComparatorLowerBound {
    public:
        PairComparatorLowerBound() = default;
        
        auto operator()(const PairT& a, const PairT& b) -> bool {
            auto key_cmpor = KeyComparatorT{};
            return key_cmpor(a.first, b.first) < 0;
        }
    };

    struct PairComparator {
    public:
        PairComparator() = default;
        
        auto operator()(const PairT& a, const PairT& b) -> int {
            auto key_cmpor = KeyComparatorT{};
            return (int)key_cmpor(a.first, b.first);
        }
    };


    using PairLowerBoundCmpT = PairComparatorLowerBound;
    using PairThreeWayCmpT = PairComparator;
public:
    struct RemoveInfo {
    public:
        RemoveInfo() = default;
        PidT child_pid;
        KeyT child_key;
    };

    InternalPage() = delete;
    InternalPage(const InternalPage& other) = delete;

    void Init() noexcept;

    void SetInitialState(PairT first_kv, PidT pid1, PidT pid2) noexcept;

    auto Insert(const KeyT& key, const ValueT& value, const PidT& pid, std::shared_ptr<InternalSplitInfoT>& split_info) -> StatusOr<InternalCase>;

    auto NoExceptGet(const KeyT& key, ValueT& res, PidT& child_pid) const -> StatusOr<InternalCase>;

    auto GetChildPidOrValue(const KeyT& key, std::variant<ValueT, PidT>& result) const -> StatusOr<InternalCase>;

    auto DoUpdateOrGetChild(const KeyT& key, const ValueT& new_val, PidT& child_pid) -> StatusOr<InternalCase>;

    auto DowncastRemoveOrGetChildPid(
        const KeyT& key, 
        KeyT& child_removed_key,
        PidT& child_pid
    ) -> StatusOr<InternalCase>;

    auto CheckOrBorrowOrMerge(
        std::shared_ptr<Page>& parent
    ) -> StatusOr<InternalCase>;

    auto GetPidMatchElem(const PairT& elem, PidT& result) const -> StatusOr<InternalCase>;

    auto dump_struct() const -> std::string;

    auto RemoveFirst() -> std::pair<PairT, PidT>;

    auto ElemAt(int idx) const -> PairT;

    auto PidAt(int idx) const -> PidT;

    auto GetIdxByPid(PidT pid) const -> int;

    void SetPairAt(int idx, PairT new_elem);

    void RemoveElemAndPidAt(int idx);

    void SetElemPairAt(int idx, PairT p);

    auto GetFirstElem() const -> PairT;

    auto DumpNodeGraphviz() const -> std::string;

private:
    void PushBack(KVPidT elem);
    void PushFront(KVPidT elem);
    auto PopBack() -> KVPidT;
    auto PopFront() -> KVPidT;
    static auto GetRawPageFirstElem(const std::shared_ptr<Page>& raw_page) -> PairT;

    // first key is invalid!
    
    //  ----  [key1]
    // [pid0] [pid1]
    std::array<PairT, SLOT_CNT> pairs;
    std::array<PidT, SLOT_CNT> pids;
};




INTERNAL_TEMPLATE_ARGUMENTS
void InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::Init() noexcept {
    BTreePage::Init(BTreePageType::INTERNAL_PAGE, (int)SLOT_CNT);
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::Insert(const KeyT& key, const ValueT& value, const PidT& pid, std::shared_ptr<InternalSplitInfoT>& split_info) -> StatusOr<InternalCase> {
    /*
        insert kv into page
        1. find position to insert
        2. memorymove
        3. check whether reaching max size
        4. if max split
    */

    // 1. find pos to insert
    auto new_pair = std::pair<KeyT, ValueT>{key, value};
    auto end_ite = std::begin(pairs) + GetSize();
    auto start_ite = std::begin(pairs);
    auto ge_ite = std::lower_bound(start_ite + 1, end_ite, new_pair, PairLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);
    // 2. memmove
    std::copy_backward(
        ge_ite,
        end_ite,
        end_ite - 1
    );
    std::copy_backward(
        std::begin(this->pids) + new_idx,
        std::begin(this->pids) + GetSize(),
        std::begin(this->pids) + GetSize() - 1
    );
    // insert
    this->pairs[new_idx] = new_pair;
    this->pids[new_idx] = pid;
    // 3. check whethre reaching max
    ChangeSizeBy(1);
    if (GetSize() < GetMaxSize()) {
        return {InternalCase::OK};        
    }

    auto new_page = RawPageMgr::create();
    auto& new_inner_page = *reinterpret_cast<SelfT*>(new_page->data());
    new_inner_page.Init();
    assert(split_info.get() != nullptr);
    split_info->new_page_id = new_inner_page.GetPageId();
    auto mid_pos = GetMinSize();
    split_info->mid_elem = this->pairs[mid_pos];

    auto& new_pairs = new_inner_page.pairs;
    auto& new_pids = new_inner_page.pids;
    std::copy(
        std::begin(this->pairs) + mid_pos + 1, 
        std::begin(this->pairs) + GetSize(), 
        std::begin(new_pairs) + 1
    );
    std::copy(
        std::begin(this->pids) + mid_pos, 
        std::begin(this->pids) + GetSize(), 
        std::begin(new_pids)
    );
    new_inner_page.SetSize(GetSize() - GetMinSize());
    this->SetSize(GetMinSize());
    return {InternalCase::InsertSplit};
}


INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::NoExceptGet(const KeyT& key, ValueT& res, PidT& child_pid) const -> StatusOr<InternalCase>{
    /*
        get value of given key in page
        1. find pos of key
        2. if equal, return value
        3. return child page id
    */
    // 1. find pos to update
    auto end_ite = std::begin(pairs) + GetSize();
    auto start_ite = std::begin(pairs);  // first is begin() + 1
    
    auto search_pair = std::pair<KeyT, ValueT>{key, ValueT{}};
    auto ge_ite = std::lower_bound(start_ite + 1, end_ite, search_pair, PairLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. if equal, return value

    // all keys < search_key
    if (ge_ite == end_ite) {
        // can not defer ge_ite
        auto pos_idx = new_idx - 1;
        auto pid = this->pids[pos_idx];
        child_pid = pid;
        return {InternalCase::GetChildPageId};
    }

    if (PairThreeWayCmpT{}(*ge_ite, search_pair) == 0) {
        // search_key == *ge_ite.key -> return value
        auto pos_idx = new_idx;
        auto& pair = this->pairs[pos_idx];
        res = pair.second;
        return {InternalCase::GetValue};
    }

    // search_key < *ge_ite -> return child_page_id
    auto pos_idx = new_idx - 1;
    auto pid = this->pids[pos_idx];
    child_pid = pid;
    return {InternalCase::GetChildPageId};
}

INTERNAL_TEMPLATE_ARGUMENTS
void InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::SetInitialState(PairT first_kv, PidT pid1, PidT pid2) noexcept {
    this->pairs[1] = first_kv;
    this->pids[0] = pid1;
    this->pids[1] = pid2;
    SetSize(2);
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::dump_struct() const -> std::string {
    std::string ret{};
    ret += std::format("------------------- [INNER] page pid: {} -----------------\n", GetPageId());
    ret += "children pid: ";
    for (int i = 0; i < GetSize(); i++) {
        ret += std::format("{}, ", this->pids[i]);
    }
    ret += "\n";
    ret += "values: ";
    for (int i = 0; i < GetSize(); i++) {
        ret += std::format("{}, ", this->pairs[i].second.dump_struct());
    }
    ret += "\n";
    for (int i = 0; i < GetSize(); i++) {
        ret += std::format("[Inner] key: {}, ", this->pairs[i].first)
            += std::format("child: {} pid: {} ", i, this->pids[i]);
        auto pid = this->pids[i];
        auto page = RawPageMgr::get_page(pid);
        if (CheckIsLeafPage(page)) {
            ret += "[Inner] Child Is LEAF!\n";
            auto btree_page = reinterpret_cast<LeafT*>(page->data());
            ret += btree_page->dump_struct();
        } else {
            ret += "[Inner] Child Is INNER!!\n";
            auto btree_page = reinterpret_cast<SelfT*>(page->data());
            ret += btree_page->dump_struct();
            ret += "[Inner] Child end!\n";
        }
    }
    return ret;
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::GetChildPidOrValue(const KeyT& key, std::variant<ValueT, PidT>& result) const -> StatusOr<InternalCase> {
    /*
        assert key not exist in elements in this->pairs
        1. find slot whose key >= search_key
        2. check duplicate (assert not duplicate)
        3. return pid[pos-1]
    */
    // 1. find slot whose key >= search_key
    auto const end_ite = std::begin(pairs) + GetSize();
    auto const start_ite = std::begin(pairs);  // first is begin() + 1
    
    auto search_pair = std::pair<KeyT, ValueT>{key, ValueT{}};
    auto ge_ite = std::lower_bound(start_ite + 1, end_ite, search_pair, PairLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. check duplicate (assert not duplicate)
    if (ge_ite == end_ite) {
        // can not defer ge_ite
        auto pos_idx = new_idx - 1;
        auto pid = this->pids[pos_idx];
        result = pid;
        return {InternalCase::GetChildPageId};
    }

    if (PairThreeWayCmpT{}(*ge_ite, search_pair) == 0) {
        // search_key == *ge_ite.key -> key duplicate
        result = ge_ite->second;
        return {InternalCase::GetValue};
    }

    // 3. return pid[pos-1]
    auto pos_idx = new_idx - 1;
    auto pid = this->pids[pos_idx];
    result = pid;
    return {InternalCase::GetChildPageId};
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::GetPidMatchElem(const PairT& elem, PidT& result) const -> StatusOr<InternalCase> {
    /*
        assert key not exist in elements in this->pairs
        1. find slot whose key >= search_key
        2. check duplicate (assert not duplicate)
        3. return pid[pos-1]
    */
    // 1. find slot whose key >= search_key
    auto const end_ite = std::begin(pairs) + GetSize();
    auto const start_ite = std::begin(pairs);  // first is begin() + 1
    
    auto search_pair = elem;
    auto ge_ite = std::lower_bound(start_ite + 1, end_ite, search_pair, PairLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. check duplicate (assert not duplicate)
    if (ge_ite == end_ite) {
        return {InternalCase::RemoveMatchPidNotFound};
    }

    if (PairThreeWayCmpT{}(*ge_ite, search_pair) == 0) {
        // search_key == *ge_ite.key -> key found
        result = this->pids[new_idx];
        return {InternalCase::OK};
    }

    return {InternalCase::RemoveMatchPidNotFound};
}


INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::DoUpdateOrGetChild(const KeyT& key, const ValueT& new_val, PidT& child_pid) -> StatusOr<InternalCase> {
    /*
        get value of given key in page
        1. find pos of key
        2. if equal, return value
        3. return child page id
    */
    // 1. find pos to update
    auto end_ite = std::begin(pairs) + GetSize();
    auto start_ite = std::begin(pairs);  // first is begin() + 1
    
    auto search_pair = std::pair<KeyT, ValueT>{key, ValueT{}};
    auto ge_ite = std::lower_bound(start_ite + 1, end_ite, search_pair, PairLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. if equal, return value

    // all keys < search_key
    if (ge_ite == end_ite) {
        // can not defer ge_ite
        auto pos_idx = new_idx - 1;
        auto pid = this->pids[pos_idx];
        child_pid = pid;
        return {InternalCase::GetChildPageId};
    }

    if (PairThreeWayCmpT{}(*ge_ite, search_pair) == 0) {
        // search_key == *ge_ite.key -> return value
        auto pos_idx = new_idx;
        auto& pair = this->pairs[pos_idx];
        pair.second = new_val;
        // std::cout << std::format("[internal update] find key in internal page pid:{} ge_ite: {}, search_pair: {}\n", GetPageId(), ge_ite->first, search_pair.first);
        return {InternalCase::OK};
    }

    // search_key < *ge_ite -> return child_page_id
    auto pos_idx = new_idx - 1;
    auto pid = this->pids[pos_idx];
    child_pid = pid;
    return {InternalCase::GetChildPageId};
}



INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::DowncastRemoveOrGetChildPid(
        const KeyT& key, 
        KeyT& child_removed_key,
        PidT& child_pid
    ) -> StatusOr<InternalCase> {
    /*
        get value of given key in page
        1. find pos of key
        2. if equal, remove, return
        3. return child page id
    */
    // 1. find pos to update
    auto end_ite = std::begin(pairs) + GetSize();
    auto start_ite = std::begin(pairs);  // first is begin() + 1
    
    auto search_pair = std::pair<KeyT, ValueT>{key, ValueT{}};
    auto ge_ite = std::lower_bound(start_ite + 1, end_ite, search_pair, PairLowerBoundCmpT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. if equal, remove, return

    // all keys < search_key
    if (ge_ite == end_ite) {
        // can not defer ge_ite
        auto pos_idx = new_idx - 1;
        auto pid = this->pids[pos_idx];
        child_pid = pid;
        return {InternalCase::GetChildPageId};
    } else if (PairThreeWayCmpT{}(*ge_ite, search_pair) != 0) {
        // search_key < *ge_ite -> return child_page_id
        auto pos_idx = new_idx - 1;
        auto pid = this->pids[pos_idx];
        child_pid = pid;
        return {InternalCase::GetChildPageId};
    }

    // 3. find elem to be removed -> borrow elem from first child
    auto pos_idx = new_idx;
    auto pid = this->pids[pos_idx];
    auto child_raw_page = RawPageMgr::get_page(pid);
    auto child_first_elem = GetRawPageFirstElem(child_raw_page);
    auto child_first_key = child_first_elem.first;
    this->pairs[pos_idx] = child_first_elem;
    child_pid = pid;
    child_removed_key = child_first_key;
    return {InternalCase::RemoveFound};
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::RemoveFirst() -> std::pair<PairT, PidT> {
    return this->pairs[1];
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::GetRawPageFirstElem(const std::shared_ptr<Page>& raw_page) -> PairT {
    if (CheckIsLeafPage(raw_page)) {
        auto& leaf = *reinterpret_cast<LeafT*>(raw_page->data());
        return leaf.GetFirstElem();
    } else {
        auto& inner = *reinterpret_cast<SelfT*>(raw_page->data());
        return inner.GetFirstElem();
    }
}


INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::CheckOrBorrowOrMerge(
    std::shared_ptr<Page>& parent
) -> StatusOr<InternalCase> {
    if (GetSize() < GetMinSize()) {
        // need borrow or merge
        auto& parent_inner = *reinterpret_cast<SelfT*>(parent->data());
        auto my_pid_idx = parent_inner.GetIdxByPid(GetPageId());
        assert(my_pid_idx != -1);
        auto simbling_pid_idx = (my_pid_idx == parent_inner.GetSize() - 1)? my_pid_idx - 1 : my_pid_idx + 1;
        auto replace_or_merge_pair_idx_in_parent = std::max(my_pid_idx, simbling_pid_idx);
        auto replace_or_merge_pair = parent_inner.ElemAt(replace_or_merge_pair_idx_in_parent);

        auto simbling_raw_page = RawPageMgr::get_page(simbling_pid_idx);
        auto& simbling_inner = *reinterpret_cast<SelfT*>(simbling_raw_page->data());

        if (simbling_inner.GetSize() > GetMinSize()){
            // can borrow

            // if simbling idx > my idx, move parent to last
            // else move parent elem to first
            if (my_pid_idx == simbling_pid_idx - 1) {
                auto sim_front_tuple = simbling_inner.PopFront();
                auto k_v_pid_tuple = std::make_tuple(
                    replace_or_merge_pair.first, 
                    replace_or_merge_pair.second, 
                    std::get<2>(sim_front_tuple)
                );

                PushBack(k_v_pid_tuple);

                parent_inner.SetPairAt(
                    replace_or_merge_pair_idx_in_parent, 
                    std::make_pair(
                        std::get<0>(sim_front_tuple),
                        std::get<1>(sim_front_tuple)
                    )
                );
            } else {
                assert(my_pid_idx == simbling_pid_idx + 1);
                auto sim_back_tuple = simbling_inner.PopBack();
                auto k_v_pid_tuple = std::make_tuple(
                    replace_or_merge_pair.first, 
                    replace_or_merge_pair.second, 
                    std::get<2>(sim_back_tuple)
                );
                PushFront(k_v_pid_tuple);
                parent_inner.SetPairAt(
                    replace_or_merge_pair_idx_in_parent, 
                    std::make_pair(
                        std::get<0>(sim_back_tuple),
                        std::get<1>(sim_back_tuple)
                    )
                );
            }
            return {InternalCase::OK};
        } else {
            // merge with simbling
            auto merger_idx = std::min(my_pid_idx, simbling_pid_idx);
            SelfT* merger_inner_ptr, *mergee_inner_ptr;
            if (merger_idx == my_pid_idx) {
                merger_inner_ptr = this;
                mergee_inner_ptr = &simbling_inner;
            } else {
                merger_inner_ptr = &simbling_inner;
                mergee_inner_ptr = this;
            }
            auto& merger_inner = *reinterpret_cast<SelfT*>(merger_inner_ptr);
            auto& mergee_inner = *reinterpret_cast<SelfT*>(mergee_inner_ptr);
            // copy parent borrow elem to merger
            auto mergee_first_pid = mergee_inner.PidAt(0);
            auto kvp_tp = std::make_tuple(replace_or_merge_pair.first, replace_or_merge_pair.second, mergee_first_pid);
            merger_inner.PushBack(kvp_tp);
            parent_inner.RemoveElemAndPidAt(replace_or_merge_pair_idx_in_parent);
            // copy mergee to merger
            std::copy(
                std::begin(mergee_inner.pairs) + 1,
                std::begin(mergee_inner.pairs) + mergee_inner.GetSize(),
                std::begin(merger_inner.pairs) + merger_inner.GetSize()
            );
            std::copy(
                std::begin(mergee_inner.pids) + 1,
                std::begin(mergee_inner.pids) + mergee_inner.GetSize(),
                std::begin(merger_inner.pids) + merger_inner.GetSize()
            );
            ChangeSizeBy(mergee_inner.GetSize() - 1);
            return {InternalCase::RemoveDidMerge};
        }
    }
    // get size > min_size
    return {InternalCase::OK};
}


INTERNAL_TEMPLATE_ARGUMENTS
void InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::RemoveElemAndPidAt(int idx) {
    if (idx + 1 != GetSize()) {
        std::copy(
            std::begin(pairs) + idx + 1,
            std::begin(pairs) + GetSize(),
            std::begin(pairs) + idx
        );
        std::copy(
            std::begin(pids) + idx + 1,
            std::begin(pids) + GetSize(),
            std::begin(pids) + idx
        );
    }
    ChangeSizeBy(-1);
}


INTERNAL_TEMPLATE_ARGUMENTS
void InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::SetElemPairAt(int idx, PairT p) {
    this->pairs[idx] = p;
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::GetFirstElem() const -> PairT {
    return this->pairs[1];
}

INTERNAL_TEMPLATE_ARGUMENTS
void InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::PushBack(KVPidT elem) {
    this->pairs[GetSize()] = std::make_pair(
        std::get<0>(elem),
        std::get<1>(elem)
    );
    this->pids[GetSize()] = std::get<2>(elem);
    ChangeSizeBy(1);
}

INTERNAL_TEMPLATE_ARGUMENTS
void InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::PushFront(KVPidT elem) {
    std::copy(
        std::begin(pairs) + 1,
        std::begin(pairs) + GetSize(),
        std::begin(pairs) + 2
    );
    std::copy(
        std::begin(pids),
        std::begin(pids) + GetSize(),
        std::begin(pids) + 1
    );

    this->pairs[1] = std::make_pair(
        std::get<0>(elem),
        std::get<1>(elem)
    );
    this->pids[0] = std::get<2>(elem);
    ChangeSizeBy(1);
}


INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::PopBack() -> KVPidT {
    auto ret = std::make_tuple(
        this->pairs[GetSize() - 1].first,
        this->pairs[GetSize() - 1].second,
        this->pids[GetSize() - 1]
    );
    ChangeSizeBy(-1);
    return ret;
}


INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::PopFront() -> KVPidT {
    auto ret = std::make_tuple(
        this->pairs[1].first,
        this->pairs[1].second,
        this->pids[0]
    );
    std::copy(
        std::begin(pairs) + 2,
        std::begin(pairs) + GetSize(),
        std::begin(pairs) + 1
    );
    std::copy(
        std::begin(pids) + 1,
        std::begin(pids) + GetSize(),
        std::begin(pids)
    );

    ChangeSizeBy(-1);
    return ret;
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::ElemAt(int idx) const -> PairT {
    return this->pairs[idx];
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::PidAt(int idx) const -> PidT {
    return this->pids[idx];
}

INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::GetIdxByPid(PidT pid) const -> int {
    for (int i = 0; i < GetSize(); i++) {
        if (this->pids[i] == pid) {
            return i;
        }
    }
    return -1;
}

INTERNAL_TEMPLATE_ARGUMENTS
void InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::SetPairAt(int idx, PairT new_elem) {
    this->pairs[idx] = new_elem;
}



INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::DumpNodeGraphviz() const -> std::string {
    std::stringstream out;
    out << "  node" << GetPageId() << " [label=\"";
    for (int i = 0; i < GetSize(); ++i) {
        if (i > 0) 
        {
            out << "|";
            out << "<f" << i << "> " << this->pairs[i].first;
        } else {
            out << "<f" << 0 << "> -" ;
        }
    }
    out << "\"];\n";

    for (int i = 0; i < GetSize(); ++i) {
        int child_id = pids[i];
        auto raw_page = RawPageMgr::get_page(child_id);
        if (CheckIsLeafPage(raw_page)) {
            auto& leaf = *reinterpret_cast<LeafT*>(raw_page->data());
            out << leaf.DumpNodeGraphviz();
        } else {
            auto& inner = *reinterpret_cast<SelfT*>(raw_page->data());
            out << inner.DumpNodeGraphviz();
        }
        out << "  node" << GetPageId() << ":f" << i << " -> node" << child_id << ";\n";
    }

    return out.str();
}