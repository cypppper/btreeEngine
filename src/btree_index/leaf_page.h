#pragma once

#include <algorithm>
#include <cstring>
#include <exception>
#include <format>
#include <string>
#include <utility>
#include <iostream>


#include "src/btree_index/common.h"
#include "src/status/status.h"
#include "src/btree_index/btree_page.h"




enum class LeafCase: int {
    SplitPage,
    OK,
};

template <typename KeyT, typename ValueT, typename KeyComparatorT>
class LeafPage: public BTreePage {
    static size_t constexpr SLOT_CNT = PAGE_SLOT_CNT_CALC<sizeof(KeyT), sizeof(ValueT)>;
    using SelfT = LeafPage<KeyT, ValueT, KeyComparatorT>;
    using LeafSplitInfo = SplitInfo<KeyT, ValueT>;
private:
    std::array<KeyT, SLOT_CNT> keys;
    std::array<ValueT, SLOT_CNT> vals;

    
public: 
    LeafPage() = delete;
    LeafPage(const LeafPage& other) = delete;

    void Init() noexcept;

    auto Insert(const KeyT& key, const ValueT& value, std::shared_ptr<LeafSplitInfo>& split_info) -> StatusOr<LeafCase>;
    auto Insert(const KeyT& key, const ValueT& value) -> Status;
    auto Update(const KeyT& key, const ValueT& value) -> Status;
    auto get(const KeyT& key) const -> StatusOr<ValueT>;
    auto remove(const KeyT& key) -> StatusOr<ValueT>;
    auto dump_struct() const -> std::string;

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
    auto const ge_ite = std::lower_bound(start_ite, end_ite, key, KeyComparatorT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. memmove
    std::copy_backward(
        ge_ite,
        end_ite,
        ge_ite + 1
    );
    std::copy_backward(
        std::begin(this->vals) + new_idx,
        std::begin(this->vals) + GetSize(),
        std::begin(this->vals) + new_idx + 1
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
auto LeafPage<KeyT, ValueT, KeyComparatorT>::Insert(const KeyT& key, const ValueT& value) -> Status {
    auto info = std::shared_ptr<LeafSplitInfo>{};
    auto ret = this->Insert(key, value, info);
    if (!ret.Ok()) {
        std::cout << "receive info" << std::format("mid elem key: {}, new page id: {}\n", info->mid_elem.first, info->new_page_id);
    }
    return ret;
}

LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::Update(const KeyT& key, const ValueT& value) -> Status {
    /*
        update kv in page
        1. find position to update
        2. if not exist, return KeyNotFound
        3. change value at this pos
    */

    // 1. find pos to update
    auto end_ite = std::begin(keys) + GetSize();
    auto start_ite = std::begin(keys);
    auto ge_ite = std::lower_bound(start_ite, end_ite, key, KeyComparatorT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. if not exist, return KeyNotFound
    if (ge_ite == end_ite) {
        // no key in leaf page >= key
        return {make_exception<KeyNotFoundException>()};
    } else if (KeyComparatorT{}(*ge_ite, key) != 0) {
        return {make_exception<KeyNotFoundException>()};
    }

    // 3. change value at this pos
    this->vals[new_idx] = value;
    return {};
}

LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::get(const KeyT& key) const -> StatusOr<ValueT>{
    /*
        get value of given key in page
        1. find pos of key
        2. if not exist return KeyNotFound
        3. return value
    */
    // 1. find pos to update
    auto end_ite = std::begin(keys) + GetSize();
    auto start_ite = std::begin(keys);
    auto ge_ite = std::lower_bound(start_ite, end_ite, key, KeyComparatorT{});
    auto new_idx = std::distance(start_ite, ge_ite);

    // 2. if not exist, return KeyNotFound
    if (ge_ite == end_ite) {
        // no key in leaf page >= key
        return {make_exception<KeyNotFoundException>()};
    } else if (KeyComparatorT{}(*ge_ite, key) != 0) {
        return {make_exception<KeyNotFoundException>()};
    }

    // 3. change value at this pos
    return {this->vals[new_idx]};
}

LEAF_TEMPLATE_ARGUMENTS
auto LeafPage<KeyT, ValueT, KeyComparatorT>::remove(const KeyT& key) -> StatusOr<ValueT>{
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
    auto ge_ite = std::lower_bound(start_ite, end_ite, key, KeyComparatorT{});
    auto new_idx = std::distance(start_ite, ge_ite);
    
    // 2. if not exist return KeyNotFound
    if (ge_ite == end_ite) {
        // no key in leaf page >= key
        return {make_exception<KeyNotFoundException>()};
    } else if (KeyComparatorT{}(*ge_ite, key) != 0) {
        return {make_exception<KeyNotFoundException>()};
    }

    // 3. remove kv in this pos
    ValueT old_value = this->vals[new_idx];
    std::copy_backward(
        ge_ite + 1,
        end_ite,
        ge_ite
    );
    std::copy_backward(
        std::begin(this->vals) + new_idx + 1,
        std::begin(this->vals) + GetSize(),
        std::begin(this->vals) + new_idx
    );
    ChangeSizeBy(-1);

    // 4. if less than min_size, merge
    if (GetSize() == GetMinSize()) {
        // TODO: MERGE with peers
        return {make_exception<OutofSpaceException>()};
    }

    // 5. return old value
    return {old_value};
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
        ret += std::format("key: {}, val: {}\n", this->keys[i], 0);
    }
    return ret;
}

