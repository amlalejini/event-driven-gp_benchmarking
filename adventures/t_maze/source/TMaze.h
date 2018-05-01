#ifndef TMAZE_H
#define TMAZE_H

#include <iostream>
#include <string>
#include <utility>
#include <algorithm>
#include <functional>
#include <deque>
#include <unordered_set>
#include <unordered_map>

#include "base/Ptr.h"
#include "base/vector.h"
#include "control/Signal.h"
#include "tools/BitVector.h"
#include "tools/Random.h"
#include "tools/random_utils.h"
#include "tools/math.h"
#include "tools/string_utils.h"

class TMaze {
  public:

    enum Facing {N=0, E, S, W};
    enum CellType {START, REWARD, DECISION, CORRIDOR};

    static constexpr size_t NUM_DIRECTIONS = 4;
    static constexpr Facing GetFacing(size_t i) { 
      emp_assert(i < NUM_DIRECTIONS); return static_cast<Facing>(i); 
    }
    static constexpr size_t GetFacing(Facing val) {
      return val;
    }

    struct Cell {
      std::unordered_map<Facing, size_t> neighbors;
      CellType type;
      double value;
    };

  protected:

    size_t corridor_len;

    emp::vector<Cell> maze;

    emp::vector<size_t> reward_cell_ids;
    size_t large_reward_cell_id; 

  public:
    TMaze() { ; }
};

#endif