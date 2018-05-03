#ifndef SGP_ADVENTURE_TOOLBELT_H
#define SGP_ADVENTURE_TOOLBELT_H

#include <iostream>
#include <string>
#include <utility>
#include <fstream>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <unordered_map>

#include "base/Ptr.h"
#include "base/vector.h"
#include "hardware/EventDrivenGP.h"
#include "tools/BitVector.h"
#include "tools/Random.h"
#include "tools/random_utils.h"
#include "tools/math.h"
#include "tools/string_utils.h"

namespace toolbelt {

  /// Generate random tags. Can guarantee uniqueness. 
  template<size_t TAG_WIDTH>
  emp::vector<typename emp::EventDrivenGP_AW<TAG_WIDTH>::affinity_t> GenerateRandomTags(emp::Random & rnd, size_t tag_cnt, bool guarantee_unique=true) {
    using tag_t = typename emp::EventDrivenGP_AW<TAG_WIDTH>::affinity_t;
    if (guarantee_unique) {
      if (tag_cnt > emp::Pow2(TAG_WIDTH)) {
        std::cout << "Number of unique tags (" << tag_cnt << ") exceeds limit! Exiting..." << std::endl;
        exit(-1);
      }
    }
    std::unordered_set<int> uset; // We'll use this to ensure all maze tags are unique.
    emp::vector<tag_t> tags;
    for (size_t i = 0; i < tag_cnt; ++i) {
      tags.emplace_back(tag_t());
      tags[i].Randomize(rnd);
      if (guarantee_unique) {
        // If we guarantee uniqueness, keep regenerating tags until we haven't seen one before. 
        int tag_int = tags[i].GetUInt(0);
        while (true) {
          if (!emp::Has(uset, tag_int)) {
            uset.emplace(tag_int);
            break;
          } else {
            tags[i].Randomize(rnd);
            tag_int = tags[i].GetUInt(0);
          }
        }
      }
    }
    return tags;
  }

}

#endif