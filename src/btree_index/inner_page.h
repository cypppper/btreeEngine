#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <iostream>

#include "src/btree_index/common.h"
#include "src/status/status.h"
#include "src/btree_index/btree_page.h"

enum class InternalCase: int {
    KeyFound,
    ChildPageId,
    OK,
};

INTERNAL_TEMPLATE_ARGUMENTS
class InternalPage: public BTreePage {
    static size_t constexpr SLOT_CNT = PAGE_SLOT_CNT_CALC<sizeof(KeyT) + sizeof(ValueT), sizeof(PidT)>;
    using LeafT = LeafPage<KeyT, ValueT, KeyComparatorT>;
    using InternalT = InternalPage<KeyT, ValueT, int, KeyComparatorT>;
    using PairT = std::pair<KeyT, ValueT>;

    struct PairComparator {
    public:
        PairComparator() = default;
        
        auto operator()(const PairT& a, const PairT& b) -> int {
            auto key_cmpor = KeyComparatorT{};
            std::cout << "[comparator] " << a.first << " " << b.first << std::endl;
            return key_cmpor(a.first, b.first);
        }
    };

    using PairComparatorT = PairComparator;
public:
    InternalPage() = delete;
    InternalPage(const InternalPage& other) = delete;

    void Init() noexcept;

    void SetInitialState(PairT first_kv, PidT pid1, PidT pid2) noexcept;

    auto Insert(const KeyT& key, const ValueT& value, const PidT& pid) -> Status;

    auto Get(const KeyT& key, ValueT& res, PidT& child_pid) const -> StatusOr<InternalCase>;

    auto dump_struct() const -> std::string;

private:
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
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::Insert(const KeyT& key, const ValueT& value, const PidT& pid) -> Status {
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
    auto ge_ite = std::lower_bound(start_ite + 1, end_ite, new_pair, PairComparatorT{});
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
    if (GetSize() == GetMaxSize()) {
        // TODO: SPLIT
        return {make_exception<OutofSpaceException>()};
    }
    return {};
}


INTERNAL_TEMPLATE_ARGUMENTS
auto InternalPage<KeyT, ValueT, PidT, KeyComparatorT>::Get(const KeyT& key, ValueT& res, PidT& child_pid) const -> StatusOr<InternalCase>{
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
    auto ge_ite = std::lower_bound(start_ite + 1, end_ite, search_pair, PairComparatorT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. if equal, return value
    if (ge_ite == end_ite) {
        // can not defer ge_ite
        auto pos_idx = new_idx - 1;
        auto pid = this->pids[pos_idx];
        child_pid = pid;
        return {InternalCase::ChildPageId};
    }

    if (PairComparatorT{}(*ge_ite, search_pair) == 0) {
        // search_key == *ge_ite.key -> return value
        auto pos_idx = new_idx;
        auto& pair = this->pairs[pos_idx];
        res = pair.second;
        return {InternalCase::OK};
    }

    // search_key < *ge_ite -> return child_page_id
    auto pos_idx = new_idx - 1;
    auto pid = this->pids[pos_idx];
    child_pid = pid;
    return {InternalCase::ChildPageId};
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
    for (int i = 0; i < GetSize(); i++) {
        ret += std::format("key: {}, ", this->pairs[i].first)
            += std::format("child: {}\n", i);
        auto pid = this->pids[i];
        auto page = RawPageMgr::get_page(pid);
        if (CheckIsLeafPage(page)) {
            auto btree_page = reinterpret_cast<LeafT*>(page->data());
            ret += btree_page->dump_struct();
        } else {
            auto btree_page = reinterpret_cast<InternalT*>(page->data());
            ret += btree_page->dump_struct();
        }
    }
    return ret;
}
