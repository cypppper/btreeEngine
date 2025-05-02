#pragma once

#include <vector>
#include <unordered_map>
#include "common.h"

INTERNAL_TEMPLATE_ARGUMENTS
class InternalPage;

LEAF_TEMPLATE_ARGUMENTS
class LeafPage;


using Page = std::vector<char>;

struct RawPageMgr {
public: 
    static auto create(int page_size = BTREE_PAGE_SIZE) -> std::shared_ptr<Page>;
    static auto get_page(int pid) -> std::shared_ptr<Page>;
private:
    static std::unordered_map<int, std::shared_ptr<Page>> pages_map;
    static int next_page_id;
};


enum class BTreePageType: int { INVALID_INDEX_PAGE = 0, LEAF_PAGE, INTERNAL_PAGE };


class BTreePage {
public:
    BTreePage() = delete;
    BTreePage(const BTreePage &other) = delete;
    ~BTreePage() = delete;
    
    void Init(BTreePageType t, int max_s) noexcept;
    void SetPageId(int pid) noexcept;
    auto GetPageId() const -> int;

    auto IsLeafPage() const -> bool;
    void SetPageType(BTreePageType page_type);
  
    auto GetSize() const -> int;
    void SetSize(int size);
    void ChangeSizeBy(int amount);
  
    auto GetMaxSize() const -> int;
    void SetMaxSize(int max_size);
    auto GetMinSize() const -> int;

private:
    // page_id
    int page_id;
    // Member variables, attributes that both internal and leaf page share
    BTreePageType page_type;
    // Number of key & value pairs in a page
    int size;
    // Max number of key & value pairs in a page
    int max_size;
};



auto CheckIsLeafPage(const std::shared_ptr<Page>& ptr) -> bool;
