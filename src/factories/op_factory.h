#pragma once

#include "common.h"
#include "src/btree_index/index.h"

#include <functional>


enum class OPTYPE: int {
    INSERT,
    GET,
    UPDATE,
    REMOVE
};

template<OPTYPE T>
struct TT{};



INDEX_TEMPLATE_ARGUMENTS
struct IndexOpFactory {
    using IndexT = Index<KeyT, ValueT, KeyComparatorT>;
public:
    template<OPTYPE  OP, typename ... Args>
    std::function<void()> make_op(std::shared_ptr<IndexT> idx, KeyT k, Args&&... args) {
        if constexpr (OP == OPTYPE::INSERT) {
            return [idx, k, &args...]() {
                idx->Insert(k, args...).Unwrap();
            };
        } else if constexpr (OP == OPTYPE::UPDATE) {
            return [idx, k, &args...]() {
                idx->Update(k, args...).Unwrap();
            };
        } else if constexpr (OP == OPTYPE::REMOVE) {
            return [idx, k, &args...]() {
                idx->Remove(k).Unwrap();
            };
        }
        return [](){};
    }

    template<>
    std::function<void()> make_op<OPTYPE::GET>(std::shared_ptr<IndexT> idx, KeyT k, std::optional<ValueT>& res) {
        return [idx, k, &res]() {
            auto v = idx->Get(k).Unwrap();
            res = v;
        };
    }


};


