# String keys vs aggregation objects

We could use `std::pair` as key in `std::unordered_map` implementing a hash function for it:

https://stackoverflow.com/a/32685618/2576671 (simple hash combine based in XOR).

https://stackoverflow.com/a/27952689/2576671 (boost hash combine and XOR limitations)

Example:

```c++
struct pair_hash {
  template <class T1, class T2>
  std::size_t operator () (const std::pair<T1,T2> &p) const {
    auto h1 = std::hash<T1>{}(p.first);
    auto h2 = std::hash<T2>{}(p.second);
    return h1 ^ (h2 << 1);
  }
};

typedef typename std::pair<std::string, std::string> pair_t;
typedef pair_t my_key_t;
typedef typename std::unordered_map<pair_t, value_t, pair_hash> my_map_t;
```

In general , we could implement hashes for multiple aggregations, for example for 3 strings:

```c++
#include <unordered_map>
#include <string>

struct ThreeStringKey {
    std::string str1;
    std::string str2;
    std::string str3;

    bool operator==(const ThreeStringKey& other) const {
        return (str1 == other.str1 && str2 == other.str2 && str3 == other.str3);
    }
};

namespace std {
    template<> struct hash<ThreeStringKey> {
        size_t operator()(const ThreeStringKey& key) const {
            size_t h1 = std::hash<std::string>()(key.str1);
            size_t h2 = std::hash<std::string>()(key.str2);
            size_t h3 = std::hash<std::string>()(key.str3);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

int main() {
    std::unordered_map<ThreeStringKey, int> myMap;
    ThreeStringKey combinedKey = {"abc", "def", "ghi"};

    myMap[combinedKey] = 42;

    return 0;
}
```



But in this project, **we decided to compose map keys as strings by mean parts aggregation with separator '#'**.

For example, to combine `method` and `uri`, we create the key string `<method>#<uri>`.

Same for `inState`, `method` and `uri`: `<inState>#<method>#<uri>`.



But, **why we decide to discard the smarter implementation ?**

The main reason is related to performance: both insertion and search are faster when using strings as keys compared to aggregated objects, probably due to efficiency of hash function implementations against string built-in hash.

The only drawback we must avoid is the keys collision, for example:

```
key(1,2) = {"ab#c","de"} => combined => ab#c#de
key(1,2) = {"ab","c#de"} => combined => ab#c#de
```

But in our case that's impossible because `method` never contains the separator and in general, for `N` parts, we guarantee that `N-1` parts will deny the use of such separator character. This is restricted in `json` schema definition (`"pattern": "^[^#]*$"`) for the following fields:

- [server|client]-provision inState
- [server|client]-provision outState
- client-endpoint id
- client-provision id

Finally, although server or client provision will not be heavy maps (at least not so big like server data storage could be), and as a consequence key object would be a reasonable alternative without real impact, for simplicity we shall inherit all our maps from a unique template based in unordered map with string key, which will be valid for all the project needs.
