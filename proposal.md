I plan to write a key-value storage engine.
In my design, Btree is chosen as index structure, and value can contain multiple type, for example
(int, double, date). 
Key Techniques:
1. template code to generate value type structure
2. regex to parse user input(like insert/update/delete)
3. customized formatter for self defined structure
4. concurrency control (multiple threads trying to modify this btree index structure)
5. iterator implemtation