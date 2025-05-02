#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <iostream>
#include <cassert>

#include "src/btree_index/common.h"
#include "src/status/status.h"
#include "src/btree_index/btree_page.h"

enum class InternalCase: int {
    KeyFound,
    GetChildPageId,
    GetValue,
    InsertSplit,
    RemoveMatchPidNotFound,
    RemoveFound,
    OK,
};

INTERNAL_TEMPLATE_ARGUMENTS
class InternalPage: public BTreePage {
    static size_t constexpr SLOT_CNT = PAGE_SLOT_CNT_CALC<sizeof(KeyT) + sizeof(ValueT), sizeof(PidT)>;
    using LeafT = LeafPage<KeyT, ValueT, KeyComparatorT>;
    using SelfT = InternalPage<KeyT, ValueT, PidT, KeyComparatorT>;
    using PairT = std::pair<KeyT, ValueT>;
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

    auto GetPidMatchElem(const PairT& elem, PidT& result) const -> StatusOr<InternalCase>;

    auto dump_struct() const -> std::string;

    auto RemoveFirst() -> std::pair<PairT, PidT>;

    auto GetIdxByPid(PidT pid) const -> int;

    auto ElemAt(int idx) const -> PairT;

    auto PidAt(int idx) const -> PidT;

    void SetElemAt(int idx, PairT new_elem);

    void RemoveElemAndPidAt(int idx);

private:
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
        ge_ite + 1
    );
    std::copy_backward(
        std::begin(this->pids) + new_idx,
        std::begin(this->pids) + GetSize(),
        std::begin(this->pids) + new_idx + 1
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
        std::cout << std::format("[internal update] find key in internal page pid:{} ge_ite: {}, search_pair: {}\n", GetPageId(), ge_ite->first, search_pair.first);
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
    auto child_first_val = child_first_elem.second;
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
        return leaf->GetFirstElem();
    } else {
        auto& inner = *reinterpret_cast<SelfT*>(raw_page->data());
        return inner.GetFirstElem();
    }
}