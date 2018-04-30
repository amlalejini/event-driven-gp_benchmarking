#ifndef ALIFE2018_TASK_LIB_H
#define ALIFE2018_TASK_LIB_H

#include <iostream>
#include <string>
#include <algorithm>
#include <functional>
#include <map>
#include <unordered_set>

#include "base/Ptr.h"
#include "base/vector.h"
#include "control/Signal.h"
#include "tools/BitVector.h"
#include "tools/Random.h"
#include "tools/random_utils.h"
#include "tools/math.h"
#include "tools/string_utils.h"
#include "tools/map_utils.h"


/// Task library for logic9 changing environment experiments.
///  - A library of tasks with common input/output types.
template<typename INPUT_T, typename OUTPUT_T>
class TaskSet {
public:
  struct Task;  // Forward declare task struct.

  using task_input_t = INPUT_T;
  using task_output_t = OUTPUT_T;
  using gen_sol_fun_t = std::function<void(Task &, const task_input_t &)>;

  struct Task {
    std::string name;
    size_t id;
    std::string desc;
    emp::vector<task_output_t> solutions;
    gen_sol_fun_t generate_solutions;

    Task(const std::string & _n, size_t _i, gen_sol_fun_t _gen_sols, const std::string & _d)
      : name(_n), id(_i), desc(_d), generate_solutions(_gen_sols)
    { ; }

  };

protected:

  emp::vector<Task> task_lib;
  std::map<std::string, size_t> name_map;

  bool sollision;                                 ///< Solution collision?
  std::unordered_set<task_output_t> solution_set; ///< Used for detecting collisions between solutions.


public:
  TaskSet() : sollision(false) { ; }
  ~TaskSet() { ; }

  const std::string & GetName(size_t id) const { return task_lib[id].name; }
  const std::string & GetDesc(size_t id) const { return task_lib[id].desc; }
  size_t GetSize() const { return task_lib.size(); }

  size_t GetID(const std::string & name) const {
    emp_assert(emp::Has(name_map, name), name);
    return emp::Find(name_map, name, (size_t)-1);
  }

  Task & GetTask(size_t id) { return task_lib[id]; }

  bool IsTask(const std::string name) const { return emp::Has(name_map, name); }

  bool IsCollision() const { return sollision; }

  void AddTask(const std::string & name,
               const gen_sol_fun_t & gen_sols,
               const std::string & desc = "")
  {
    const size_t id = task_lib.size();
    task_lib.emplace_back(name, id, gen_sols, desc);
    name_map[name] = id;
  }


  /// Set inputs. Reset everything.
  void SetInputs(const task_input_t & inputs) {
    sollision = false;
    solution_set.clear();
    for (size_t i = 0; i < task_lib.size(); ++i) {
      task_lib[i].solutions.resize(0);
      task_lib[i].generate_solutions(task_lib[i], inputs);
      for (size_t s = 0; s < task_lib[i].solutions.size(); ++s) {
        if (emp::Has(solution_set, task_lib[i].solutions[s])) {
          sollision = true;
        } else {
          solution_set.emplace(task_lib[i].solutions[s]);
        }
      }
    }
  }

  // TODO: Add CheckTask functions... (return bool if given sol is solution for a task)
  bool CheckTask(size_t task_id, const task_output_t & sol) {
    emp_assert(task_id < task_lib.size());
    Task & task = task_lib[task_id];
    for (size_t s = 0; s < task.solutions.size(); ++s) {
      if (task.solutions[s] == sol) return true;
    }
    return false;
  }

};

#endif
