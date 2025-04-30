enum class BTreePageType { INVALID_INDEX_PAGE = 0, LEAF_PAGE, INTERNAL_PAGE };

class BTreePage {
public:
    BTreePage() = delete;
    BTreePage(const BTreePage &other) = delete;
    ~BTreePage() = delete;
  
    auto IsLeafPage() const -> bool;
    void SetPageType(BTreePageType page_type);
  
    auto GetSize() const -> int;
    void SetSize(int size);
    void ChangeSizeBy(int amount);
  
    auto GetMaxSize() const -> int;
    void SetMaxSize(int max_size);
    auto GetMinSize() const -> int;
  
private:
    // Member variables, attributes that both internal and leaf page share
    BTreePageType page_type;
    // Number of key & value pairs in a page
    int size;
    // Max number of key & value pairs in a page
    int max_size;
};


template<typename Key, typename Value, typename Comparator>
class LeafPage: BTreePage {

};