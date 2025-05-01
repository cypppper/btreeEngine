#include "src/btree_index/btree_page.h"

std::unordered_map<int, std::shared_ptr<Page>> RawPageMgr::pages_map = {};
int RawPageMgr::next_page_id = 0;

auto RawPageMgr::create(int page_size) -> std::shared_ptr<Page> {
    int page_id = next_page_id++;
    auto page = std::make_shared<Page>(std::vector<char>(page_size));

    auto& btree_page = *reinterpret_cast<BTreePage*>(page->data());
    btree_page.SetPageId(page_id);

    pages_map.insert({page_id, page});

    return page;
}

auto RawPageMgr::get_page(int pid) -> std::shared_ptr<Page> {
    auto res = pages_map.find(pid);
    if (res == pages_map.end()) {
        return {};
    }
    return res->second;
}

void BTreePage::Init(BTreePageType t, int _m_size) noexcept {
    this->page_type = t;
    this->max_size = _m_size;
    this->size = 0;
}

void BTreePage::SetPageId(int pid) noexcept {
    this->page_id = pid;
}

auto BTreePage::GetPageId() const -> int {
    return this->page_id;
}

auto BTreePage::IsLeafPage() const -> bool {
    return this->page_type == BTreePageType::LEAF_PAGE;
}

void BTreePage::SetPageType(BTreePageType page_type) {
    this->page_type = page_type;
}

auto BTreePage::GetSize() const -> int {
    return this->size;
}

void BTreePage::SetSize(int size) {
    this->size = size;
}

void BTreePage::ChangeSizeBy(int amount) {
    this->size += amount;
}

auto BTreePage::GetMaxSize() const -> int {
    return this->max_size;
}

void BTreePage::SetMaxSize(int max_size) {
    this->max_size = max_size;
}

auto BTreePage::GetMinSize() const -> int {
    if (this->page_type == BTreePageType::LEAF_PAGE) {
        return (this->max_size - 1) / 2;
    }
    return (this->max_size - 1) / 2;
}


auto CheckIsLeafPage(const std::shared_ptr<Page>& ptr) -> bool {
    return reinterpret_cast<BTreePage*>(ptr->data())->IsLeafPage();
}
