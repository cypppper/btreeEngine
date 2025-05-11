# A Btree Index Storage Engine



A Btree Index Storage Engine is a simple storage engine that uses a Btree to store data. It is a simple implementation of a Btree that can be used to store data.




* Using C++20

* support:
    - MacOS
    - Linux

* feature:
    - \[OP factory\]: insert get update remove op factory.
    - \[concurrency\]: use shared_mutex to support read-write concurrency.
    - \[format\]: use fmt::format to customize formatter for self-defined struct.
    - \[meta programming\]: meta programming for template type in BTreeIndex.
    - \[variant\]: use variant in StatusOr, which can either return value or throw runtime exception.
    - \[Graphviz\]: generate btree structure into a dot file (default directory is ./dots).
        - Example:
            ![dots_example](./pics/dots_example.png "dots example")
        - Visualize it In [Graphviz Online](https://www.google.com/url?sa=t&source=web&rct=j&opi=89978449&url=https://dreampuf.github.io/GraphvizOnline/&ved=2ahUKEwi5-eC9pIiNAxXJmYkEHUQgBiUQFnoECAoQAQ&usg=AOvVaw2Sw6OnaIb_oZkOtu44VcNz)
# Build & Run:

```shell
    ./build.sh
```
