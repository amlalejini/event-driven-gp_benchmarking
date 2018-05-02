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

// TODO: function/class member documentation!

/// Class to represent a T-maze
class TMaze {
  public:

    enum Facing {N=0, E, S, W};
    enum CellType {START=0, REWARD, DECISION, CORRIDOR};
    static constexpr size_t NUM_DIRECTIONS = 4;

    // A few utility functions dealing with maze facing
    static constexpr Facing GetFacing(size_t i) { 
      emp_assert(i < NUM_DIRECTIONS); return static_cast<Facing>(i); 
    }
    static constexpr size_t GetFacing(Facing val) {
      return val;
    }
    static std::string FacingToString(Facing facing) {
      switch(facing) {
        case Facing::N:
          return "N";
        case Facing::E:
          return "E";
        case Facing::S:
          return "S";
        case Facing::W:
          return "W";
      }
    }
    static std::string CellTypeToString(CellType type) {
      switch(type) {
        case START:
          return "START";
        case REWARD:
          return "REWARD";
        case DECISION:
          return "DECISION";
        case CORRIDOR:
          return "CORRIDOR";
      }
    }

    struct Cell {
      std::unordered_map<Facing, size_t> neighbors;
      CellType type;
      double value;

      const std::unordered_map<Facing, size_t> & GetNeighbors() const { return neighbors; }
      CellType GetType() const { return type; }
      double GetValue() const { return value; }

    };

  protected:

    size_t corridor_len;

    emp::vector<Cell> maze;

    emp::vector<size_t> reward_cell_ids;
    size_t large_reward_cell_id; 
    size_t start_cell_id;
    size_t junction_cell_id;

    double small_reward_val;
    double large_reward_val;

    void BuildMaze() {
      size_t maze_id = 0;

      // Build bottom part of T: start --> corridor
      Cell & start_cell = maze[maze_id];
      start_cell_id = maze_id;
      start_cell.type = CellType::START;
      
      ++maze_id; // Move to next cell. 
      start_cell.neighbors.emplace(Facing::N, maze_id);
      
      // Build 1st corridor
      for (size_t cID = 0; cID < corridor_len; ++cID) {
        Cell & corridor_cell = maze[maze_id]; 
        corridor_cell.type = CellType::CORRIDOR;
        corridor_cell.neighbors.emplace(Facing::S, maze_id-1);
        ++maze_id;
        corridor_cell.neighbors.emplace(Facing::N, maze_id);  
      }

      // Build T Junction
      Cell & junction_cell = maze[maze_id];
      junction_cell_id = maze_id;
      junction_cell.type = CellType::DECISION;
      junction_cell.neighbors.emplace(Facing::S, maze_id-1);
      ++maze_id; // Move to next cell (we'll build left, then right)
      junction_cell.neighbors.emplace(Facing::W, maze_id);

      // Build upper-left corridor ([R<--West--D]---->R)
      for (size_t cID = 0; cID < corridor_len; ++cID) {
        Cell & corridor_cell = maze[maze_id];
        corridor_cell.type = CellType::CORRIDOR;
        corridor_cell.neighbors.emplace(Facing::E, maze_id-1);
        ++maze_id;
        corridor_cell.neighbors.emplace(Facing::W, maze_id);
      }

      // Cap off with reward cell. 
      Cell & rew_cell1 = maze[maze_id];
      reward_cell_ids[0] = maze_id;
      rew_cell1.type = CellType::REWARD;
      rew_cell1.neighbors.emplace(Facing::E, maze_id-1);
      ++maze_id;

      // Build upper-right corridor (R<----[D--EAST-->R])
      junction_cell.neighbors.emplace(Facing::E, maze_id);
      for (size_t cID = 0; cID < corridor_len; ++cID) {
        Cell & corridor_cell = maze[maze_id];
        corridor_cell.type = CellType::CORRIDOR;
        corridor_cell.neighbors.emplace(Facing::W, ((cID) ? (maze_id - 1) : junction_cell_id) );//junction_cell_id+cID);
        ++maze_id;
        corridor_cell.neighbors.emplace(Facing::E, maze_id);
      }

      // Cap off with reward cell.
      Cell & rew_cell2 = maze[maze_id];
      reward_cell_ids[1] = maze_id;
      rew_cell2.type = CellType::REWARD;
      rew_cell2.neighbors.emplace(Facing::W, maze_id-1);

      large_reward_cell_id = reward_cell_ids[0];
      // TODO: set reward
      ResetRewards();

    }

  public:
    TMaze(size_t _corridor_len = 3, double _s_reward_val = 1, double _l_reward_val = 2) 
      : corridor_len(_corridor_len), 
        maze((3 * corridor_len) + 4),
        reward_cell_ids(2),
        large_reward_cell_id(0),
        start_cell_id(0),
        junction_cell_id(0),
        small_reward_val(_s_reward_val), large_reward_val(_l_reward_val)
    { 
      BuildMaze(); 
    }

    size_t GetSize() const { return maze.size(); }

    size_t GetCorridorLen() const { return corridor_len; }

    size_t GetLargeRewardCellID() const { return large_reward_cell_id; }
    size_t GetStartCellID() const { return start_cell_id; }
    size_t GetJunctionCellID() const { return junction_cell_id; }
    const emp::vector<size_t> & GetRewardCellIDs() const { return reward_cell_ids; }

    double GetSmallRewardValue() const { return small_reward_val; }
    double GetLargeRewardValue() const { return large_reward_val; }

    Cell & GetCell(size_t id) {
      emp_assert(id < maze.size());
      return maze[id];
    }

    void Resize(size_t _corridor_len) {
      corridor_len = _corridor_len;
      maze.clear();
      maze.resize((3*corridor_len) + 4); 
      BuildMaze();
    }

    void ResetRewards() {
       // Zero out e'rybody's values! 
      for (size_t i = 0; i < maze.size(); ++i) { maze[i].value = 0; }
      // Set reward cell values. 
      for (size_t r = 0; r < reward_cell_ids.size(); ++r) {
        const size_t rID = reward_cell_ids[r];
        maze[rID].value = ((rID == large_reward_cell_id) ? large_reward_val : small_reward_val);
      }
    }

    void ClearRewards() {
      for (size_t r = 0; r < reward_cell_ids.size(); ++r) {
        maze[reward_cell_ids[r]].value = 0;
      }
    }

    void RandomizeRewards(emp::Random & rnd) {
      large_reward_cell_id = rnd.GetUInt(reward_cell_ids.size());
      ResetRewards();
    }

    void SwitchRewards() {
      large_reward_cell_id = (large_reward_cell_id == reward_cell_ids[0]) ? reward_cell_ids[1] : reward_cell_ids[0];
      ResetRewards();
    }

    void Print(std::ostream & os=std::cout) {
      // Print as linked list.
      os << "============= T-MAZE =============\n";
      os << "Maze cell count: " << maze.size() << "\n"; 
      os << "----------\n"; 
      for (size_t i = 0; i < maze.size(); ++i) {
        os << "-- Cell " << i << " --\n";
        Cell & cell = maze[i];
        os << "  Cell type: " << CellTypeToString(cell.type) << "\n";
        os << "  Cell value: " << cell.value << "\n";
        os << "  Neighbors:";
        for (size_t n = 0; n < NUM_DIRECTIONS; ++n) {
          Facing facing = GetFacing(n);
          os << " " << FacingToString(facing) << ":";
          if (emp::Has(cell.neighbors, facing)) {
            os << cell.neighbors[facing];
          } else {
            os << "NONE";
          }
        } os << "\n";
      }
    }

};

#endif