# About MAP

All three classes implement the Map interface and offer mostly the same functionality. The most important difference is the order in which iteration through the entries will happen:

## HashMap

`HashMap` makes absolutely no guarantees about the iteration order. It can (and will) even change completely when new elements are added.

- It has pair values(keys,values)
- NO duplication key values
- unordered unsorted
- it allows one null key and more than one null values



## HashTable

 `HashTable` is the generic name for hash-based maps.

- same as hash map
- it does not allows null keys and null values



## TreeMap

`TreeMap` will iterate according to the "natural ordering" of the keys according to their compareTo() method (or an externally supplied Comparator). Additionally, it implements the SortedMap interface, which contains methods that depend on this sort order.

- ordered and sortered version
- based on hashing data structures



## LinkedHashMap

 `LinkedHashMap` will iterate in the order in which the entries were put into the map.

- It is ordered version of map implementation
- Based on linked list and hashing data structures



# Visual Presentation among them

╔══════════════╦═════════════════════╦═══════════════════╦═════════════════════╗
║   Property   ║       HashMap       ║      TreeMap      ║     LinkedHashMap   ║
╠══════════════╬═════════════════════╬═══════════════════╬═════════════════════╣
║ Iteration    ║  no guarantee order ║ sorted according  ║                     ║
║   Order      ║ will remain constant║ to the natural    ║    insertion-order  ║
║              ║      over time      ║    ordering       ║                     ║
╠══════════════╬═════════════════════╬═══════════════════╬═════════════════════╣
║  Get/put     ║                     ║                   ║                     ║
║   remove     ║         O(1)        ║      O(log(n))    ║         O(1)        ║
║ containsKey  ║                     ║                   ║                     ║
╠══════════════╬═════════════════════╬═══════════════════╬═════════════════════╣
║              ║                     ║   NavigableMap    ║                     ║
║  Interfaces  ║         Map         ║       Map         ║         Map         ║
║              ║                     ║    SortedMap      ║                     ║
╠══════════════╬═════════════════════╬═══════════════════╬═════════════════════╣
║              ║                     ║                   ║                     ║
║     Null     ║       allowed       ║    only values    ║       allowed       ║
║ values/keys  ║                     ║                   ║                     ║
╠══════════════╬═════════════════════╩═══════════════════╩═════════════════════╣
║              ║   Fail-fast behavior of an iterator cannot be guaranteed      ║
║   Fail-fast  ║ impossible to make any hard guarantees in the presence of     ║
║   behavior   ║           unsynchronized concurrent modification              ║
╠══════════════╬═════════════════════╦═══════════════════╦═════════════════════╣
║              ║                     ║                   ║                     ║
║Implementation║      buckets        ║   Red-Black Tree  ║    double-linked    ║
║              ║                     ║                   ║       buckets       ║
╠══════════════╬═════════════════════╩═══════════════════╩═════════════════════╣
║      Is      ║                                                               ║
║ synchronized ║              implementation is not synchronized               ║
╚══════════════╩═══════════════════════════════════════════════════════════════╝