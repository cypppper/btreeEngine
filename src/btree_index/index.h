#include "src/status/status.h"

template<typename Key, typename Value, typename Comparator>
class Index {
public:
    auto Insert(Key&& key, Value&& val) -> Status {

    }
    auto Update(Key&& key, Value&& new_val) -> Status {

    }
    auto get(Key&& key) -> StatusOr<Value> {}
    auto remove(Key&& key) -> StatusOr<Value> {}
private:

};