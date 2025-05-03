rm -rf build
rm -rf dots
mkdir dots
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang ..
make
cd ..
./build/btree_engine
