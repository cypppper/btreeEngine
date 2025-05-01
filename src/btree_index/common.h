#pragma once

#include <cstddef>

const size_t BTREE_PAGE_SIZE = 4096;
const size_t LEAF_PAGE_HEADER_SIZE = 16;

template<int keysize, int val_size>
int constexpr PAGE_SLOT_CNT_CALC = (BTREE_PAGE_SIZE - LEAF_PAGE_HEADER_SIZE) / (keysize + val_size);


#define LEAF_TEMPLATE_ARGUMENTS template<typename KeyT, typename ValueT, typename KeyComparatorT>
#define INTERNAL_TEMPLATE_ARGUMENTS template<typename KeyT, typename ValueT, typename PidT, typename KeyComparatorT>
#define INDEX_TEMPLATE_ARGUMENTS template<typename KeyT, typename ValueT, typename KeyComparatorT>

