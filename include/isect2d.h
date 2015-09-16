#pragma once

#include <cmath>
#include <set>
#include <unordered_set>
#include <vector>
#include <array>
#include <functional> // for hash function

#include "aabb.h"
#include "obb.h"


namespace std {
    template <>
        struct hash<pair<int32_t, int32_t>> {
        size_t operator()(const pair<int32_t,int32_t>& k) const {
            return hash<int64_t>()(int64_t(k.first) << 32 | k.second);
        }
    };
}

namespace isect2d {

template<typename T>
static constexpr const T clamp(T val, T min, T max) {
    return val < min ? min
         : val > max ? max : val;
}

template<typename V>
struct ISect2D {

    struct Pair {
    Pair(int32_t _a, int32_t _b, int _next) : first(_a), second(_b), next(_next) {}
        int32_t first;
        int32_t second;

        int next;
    };

    const int_fast32_t split_x;
    const int_fast32_t split_y;
    const int_fast32_t res_x;
    const int_fast32_t res_y;
    const int_fast32_t xpad;
    const int_fast32_t ypad;

    std::vector<std::vector<int32_t>> gridAABBs;
    std::vector<Pair> pairs;
    std::vector<int> pairMap;

    ISect2D(const V _split, const V _resolution, size_t collisionHashSize = 2048)
      : split_x(_split.x), split_y(_split.y),
        res_x(_resolution.x), res_y(_resolution.y),
        xpad(ceil(res_x / split_x)),
        ypad(ceil(res_y / split_y)) {

        gridAABBs.resize(split_x * split_y);
        pairMap.assign(collisionHashSize, -1);
    }


  void clear() {
      pairs.clear();
      pairMap.assign(pairMap.size(), -1);
  }

/*
 * Performs broadphase collision detecction on _aabbs dividing the
 * screen size _resolution by _split on X and Y dimension Returns the
 * set of colliding pairs in the _aabbs container
 */
   void intersect(const std::vector<AABB<V>>& _aabbs) {
      using i32 = int_fast32_t;

      size_t index = 0;
      for (const auto& aabb : _aabbs) {
          i32 x1 = aabb.min.x / xpad;
          i32 y1 = aabb.min.y / ypad;
          i32 x2 = aabb.max.x / xpad + 1;
          i32 y2 = aabb.max.y / ypad + 1;

          x1 = clamp(x1, i32(0), split_x-1);
          y1 = clamp(y1, i32(0), split_y-1);
          x2 = clamp(x2, i32(1), split_x);
          y2 = clamp(y2, i32(1), split_y);

          for (i32 y = y1; y < y2; y++) {
              for (i32 x = x1; x < x2; x++) {
                  gridAABBs[x + y * split_x].push_back(index);
              }
          }

          index++;
      }

      for (auto& v : gridAABBs) {
          if (v.empty()) {continue;}
          // check all items against each other
          for (size_t j = 0; j < v.size()-1; ++j) {
              const auto& a(_aabbs[v[j]]);

              for (size_t k = j + 1; k < v.size(); ++k) {
                  const auto& b(_aabbs[v[k]]);

                  if (a.intersect(b)) {
                      size_t key = hash_key(hash_int(v[j])<<32 | hash_int(v[k]));
                      key &= (pairMap.size()-1);

                      int i = pairMap[key];

                      while (i != -1) {
                          if (pairs[i].first == v[j] && pairs[i].second == v[k]) {
                              // found
                              break;
                          }
                          i = pairs[i].next;
                      }

                      if (i == -1) {
                          pairs.push_back(Pair{v[j], v[k], pairMap[key]});
                          pairMap[key] = pairs.size()-1;
                      }
                  }
              }
          }
          v.clear();
      }
   }

private:
    // from fontstash
    static size_t hash_int(uint32_t a) {
        a += ~(a<<15);
        a ^=  (a>>10);
        a +=  (a<<3);
        a ^=  (a>>6);
        a += ~(a<<11);
        a ^=  (a>>16);
        return a;
    }

    // from https://gist.github.com/badboy/6267743
    static size_t hash_key(size_t key) {
        key = (~key) + (key << 18);
        key = key ^ (key >> 31);
        key = key * 21;
        key = key ^ (key >> 11);
        key = key + (key << 6);
        key = key ^ (key >> 22);
        return key;
    }

};


template<typename V>
inline static std::unordered_set<std::pair<int, int>> intersect(const std::vector<AABB<V>>& _aabbs,
                                                                V _split, V _resolution) {
#if 0
    std::set<std::pair<int, int>> pairs;

    if (_aabbs.size() == 0) {
        return pairs;
    }

    for (size_t i = 0; i < _aabbs.size(); ++i) {
        for (size_t j = i + 1; j < _aabbs.size(); ++j) {
            if (_aabbs[i].intersect(_aabbs[j])) {
                pairs.insert({ i, j });
            }
        }
    }

    return pairs;

#else

    struct AABBPair {
        const AABB<V>* aabb;
        unsigned int index;
    };

    std::unordered_set<std::pair<int, int>> pairs;
    int n = int(_split.x * _split.y);
    std::vector<AABBPair>* gridAABBs = new std::vector<AABBPair>[n];

    const short xpad = short(ceilf(_resolution.x / _split.x));
    const short ypad = short(ceilf(_resolution.y / _split.y));

    short x = 0, y = 0;

    for (int j = 0; j < _split.y; ++j) {
        for (int i = 0; i < _split.x; ++i) {
            AABB<V> cell(x, y, x + xpad, y + ypad);

            for (unsigned int index = 0; index < _aabbs.size(); ++index) {
                const AABB<V>* aabb = &_aabbs[index];
                // test the aabb against the current grid cell
                if (cell.intersect(*aabb)) {
                    gridAABBs[int(i + j * _split.x)].push_back({aabb, index});
                }
            }
            x += xpad;

            if (x >= _resolution.x) {
                x = 0;
                y += ypad;
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        auto& v = gridAABBs[i];

        for (size_t j = 0; j < v.size(); ++j) {
            for (size_t k = j + 1; k < v.size(); ++k) {

                if (v[j].aabb->intersect(*v[k].aabb)) {
                    pairs.insert({ v[j].index, v[k].index });
                }
            }
        }
    }

    delete[] gridAABBs;
    return std::move(pairs);
#endif
}

/*
 * Performs bruteforce broadphase collision detection on _aabbs
 * Returns the set of colliding pairs in the _aabbs container
 */
template<typename V>
inline static std::set<std::pair<int, int>> intersect(const std::vector<AABB<V>>& _aabbs) {
    std::set<std::pair<int, int>> pairs;

    if (_aabbs.size() == 0) {
        return pairs;
    }

    for (size_t i = 0; i < _aabbs.size(); ++i) {
        for (size_t j = i + 1; j < _aabbs.size(); ++j) {
            if (_aabbs[i].intersect(_aabbs[j])) {
                pairs.insert({ i, j });
            }
        }
    }

    return pairs;
}

}
