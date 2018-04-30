#ifndef ALIFE2018_EXPERIMENT_H
#define ALIFE2018_EXPERIMENT_H

// @includes
#include <iostream>
#include <string>
#include <utility>
#include <fstream>
#include <sys/stat.h>
#include <algorithm>
#include <functional>
#include <deque>
#include <unordered_set>

#include "base/Ptr.h"
#include "base/vector.h"
#include "control/Signal.h"
#include "Evolve/World.h"
#include "Evolve/Resource.h"
#include "Evolve/SystematicsAnalysis.h"
#include "Evolve/World_output.h"
#include "hardware/EventDrivenGP.h"
#include "hardware/InstLib.h"
#include "tools/BitVector.h"
#include "tools/Random.h"
#include "tools/random_utils.h"
#include "tools/math.h"
#include "tools/string_utils.h"

#include "dol-config.h"
#include "SGPDeme.h"
#include "TaskSet.h"

constexpr size_t RUN_ID__EXP = 0;
constexpr size_t RUN_ID__ANALYSIS = 1;

constexpr size_t TAG_WIDTH = 16;

constexpr uint32_t MIN_TASK_INPUT = 1;
constexpr uint32_t MAX_TASK_INPUT = 1000000000;
constexpr size_t MAX_TASK_NUM_INPUTS = 2;

constexpr size_t TASK_CNT = 9;

constexpr size_t TRAIT_ID__ACTIVE = 0;
constexpr size_t TRAIT_ID__LAST_TASK = 1;
constexpr size_t TRAIT_ID__DEME_ID = 2;
constexpr size_t TRAIT_ID__UID = 3;
constexpr size_t TRAIT_ID__DIR = 4;
constexpr size_t TRAIT_ID__ROLE_ID = 5;

constexpr int NO_TASK = -1;

/// Class to manage ALIFE2018 changing environment (w/logic 9) experiments.
class Experiment {
public:
  // Forward declarations.
  struct Agent;
  // Hardware/agent aliases.
  using hardware_t = emp::EventDrivenGP_AW<TAG_WIDTH>;
  using program_t = hardware_t::Program;
  using state_t = hardware_t::State;
  using inst_t = hardware_t::inst_t;
  using inst_lib_t = hardware_t::inst_lib_t;
  using event_t = hardware_t::event_t;
  using event_lib_t = hardware_t::event_lib_t;
  using memory_t = hardware_t::memory_t;
  using tag_t = hardware_t::affinity_t;
  using exec_stk_t = hardware_t::exec_stk_t;

  // World alias
  using world_t = emp::World<Agent>;
  // Task aliases
  using task_io_t = uint32_t;

  /// Agent to be evolved.
  struct Agent {
    size_t agent_id;
    program_t program;

    Agent(const program_t & _p) : agent_id(0), program(_p) { ; }
    Agent(const Agent && in) : agent_id(in.GetID()), program(in.program) { ; }
    Agent(const Agent & in): agent_id(in.GetID()), program(in.program) { ; }

    size_t GetID() const { return agent_id; }
    void SetID(size_t id) { agent_id = id; }

    program_t & GetGenome() { return program; }

  };

  /// Wrapper around SGPDeme that includes useful propagule/activation functions.
  class DOLDeme : public SGPDeme {
  public:
    using grid_t = SGPDeme::grid_t;
    using hardware_t = SGPDeme::hardware_t;
  protected:

    emp::Signal<void(hardware_t &)> on_propagule_activate_sig; // Triggered when a propagule is activated.
    
    size_t phen_id;

  public:
    DOLDeme(size_t _w, size_t _h, emp::Ptr<emp::Random> _rnd, emp::Ptr<inst_lib_t> _ilib, emp::Ptr<event_lib_t> _elib)
    : SGPDeme(_w, _h, _rnd, _ilib, _elib), phen_id(0)
    {
      for (size_t i = 0; i < grid.size(); ++i) {
        grid[i].SetTrait(TRAIT_ID__ACTIVE, 0);
        grid[i].SetTrait(TRAIT_ID__DEME_ID, i);
      }
    }

    emp::SignalKey OnPropaguleActivation(const std::function<void(hardware_t &)> & fun) { return on_propagule_activate_sig.AddAction(fun); }

    bool IsActive(size_t id) const { return (bool)grid[id].GetTrait(TRAIT_ID__ACTIVE); }
    void Activate(size_t id) { grid[id].SetTrait(TRAIT_ID__ACTIVE, 1); }
    void Deactivate(size_t id) { grid[id].SetTrait(TRAIT_ID__ACTIVE, 0); }

    size_t GetLastTask(size_t id) const { return (size_t)grid[id].GetTrait(TRAIT_ID__LAST_TASK); }
    void SetLastTask(size_t id, size_t task_id) { grid[id].SetTrait(TRAIT_ID__LAST_TASK, task_id); }

    size_t GetPhenID() const { return phen_id; }
    void SetPhenID(size_t id) { phen_id = id; }

    void ActivateDemePropagule(size_t prop_size=1, bool clumpy=false) {
      emp_assert(prop_size <= grid.size());
      if (clumpy) {
        size_t hw_id = random->GetUInt(0, grid.size());
        size_t prop_cnt = 0;
        size_t dir = 0;
        while (prop_cnt < prop_size) {
          if (!IsActive(hw_id)) {
            // Activate(hw_id); 
            prop_cnt += 1;
            on_propagule_activate_sig.Trigger(grid[hw_id]);
          } else {
            size_t r_dir = (dir + 1) % SGPDeme::NUM_DIRS;
            size_t r_id = GetNeighborID(hw_id, r_dir);
            if (!IsActive(r_id)) {
              dir = r_dir; hw_id = r_id;
            } else {
              hw_id = GetNeighborID(hw_id, dir);
            }
          }
        }
      } else {
        // We need to activate a number of hardwares equal to DEME_PROP_SIZE
        emp::Shuffle(*random, schedule);
        for (size_t i = 0; i < prop_size; ++i) {
          // Activate(schedule[i]);
          on_propagule_activate_sig.Trigger(grid[schedule[i]]);
        }
      }
    }

    void PrintActive(std::ostream & os=std::cout) {
      os << "-- Deme Active/Inactive --\n";
      for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
          os << (int)IsActive(GetID(x,y)) << " ";
        } os << "\n";
      }
    }
  };

  struct Phenotype {
    emp::vector<size_t> deme_tasks_cnts;        ///< Deme-wide task completion counts (indexed by task).
    emp::vector<size_t> indiv_tasks_cnts;       ///< Task completion broken down by individual and task.
    emp::vector<size_t> indiv_total_tasks_cnts; ///< Total task completion broken down by individual.
    emp::vector<size_t> task_switches;          ///< Task switches broken down by individual.
    size_t task_total;
    size_t total_task_switches;
    
    double score;

    Phenotype()
      : deme_tasks_cnts(0),
        indiv_tasks_cnts(0),
        indiv_total_tasks_cnts(0),
        task_switches(0),
        task_total(0),
        total_task_switches(0),
        score(0)
    { ; }

    /// Given hw id and task id, return appropriate index.
    size_t IndivTaskIndex(size_t hw_id, size_t task_id) const { return (hw_id*TASK_CNT) + task_id; }

    size_t GetIndivTaskCnt(size_t hw_id, size_t task_id) const { return indiv_tasks_cnts[IndivTaskIndex(hw_id, task_id)]; }

    size_t GetIndivTotalTaskCnt(size_t hw_id) const { return indiv_total_tasks_cnts[hw_id]; }

    size_t GetIndivTaskSwitches(size_t hw_id) const { return task_switches[hw_id]; }

    size_t GetDemeTaskCnt(size_t task_id) const { return deme_tasks_cnts[task_id]; }

    size_t GetDemeTotalTaskCnt() const { return task_total; }

    size_t GetDemeTaskSwitches() const { return total_task_switches; }

    double GetScore() const { return score; }

    double CalcMutInfo() {
      if (task_total == 0) return 0.0;

      // I(N,M) = SUM{p_ij * ln(p_ij/(p_i*p_j))}
      const size_t deme_size = indiv_total_tasks_cnts.size();
      const size_t task_cnt = deme_tasks_cnts.size();

      emp::vector<double> Pij(deme_size * task_cnt, 0);
      emp::vector<double> Pj(task_cnt, 0);
      double Pi = 0.0;
      emp::vector<size_t> workers;
      emp::vector<size_t> tasks_done;

      // Calculate Pij
      for (size_t tID = 0; tID < task_cnt; ++tID) {
        if (deme_tasks_cnts[tID] > 0) tasks_done.emplace_back(tID);
      }
      for (size_t hwID = 0; hwID < deme_size; ++hwID) {
        if (indiv_total_tasks_cnts[hwID] > 0) {
          workers.emplace_back(hwID);
          for (size_t ti = 0; ti < tasks_done.size(); ++ti) {
            const size_t j = tasks_done[ti];
            Pij[IndivTaskIndex(hwID, j)] = ((double)indiv_tasks_cnts[IndivTaskIndex(hwID, j)])/((double)indiv_total_tasks_cnts[hwID]);
          }          
        }
      } 
      for (size_t i = 0; i < Pij.size(); ++i) Pij[i] /= workers.size(); // Normalize by number of agents that did work.

      // Calculate Pi
      emp_assert(workers.size());
      Pi = 1.0/((double)workers.size()); // Same for all agents. 
      
      // Calculate Pj
      for (size_t ti = 0; ti < tasks_done.size(); ++ti) {
        const size_t j = tasks_done[ti];
        Pj[j] = ((double)deme_tasks_cnts[j])/((double)task_total);
      }

      // Calculate mutual information
      double I = 0.0;
      for (size_t ti = 0; ti < tasks_done.size(); ++ti) {
        const size_t j = tasks_done[ti];
        for (size_t wi = 0; wi < workers.size(); ++wi) {
          const size_t i = workers[wi];
          const double pij = Pij[IndivTaskIndex(i,j)];
          const double pj = Pj[j];
          emp_assert(Pj[j] > 0);
          I += (pij > 0) ? pij * emp::Ln(pij/(Pi*pj)) : 0;
        }
      }
      return I;
    }

    void Reset() {
      score = 0;
      task_total = 0;
      total_task_switches = 0;
      for (size_t i = 0; i < deme_tasks_cnts.size(); ++i) deme_tasks_cnts[i] = 0;
      for (size_t i = 0; i < indiv_tasks_cnts.size(); ++i) indiv_tasks_cnts[i] = 0;
      for (size_t i = 0; i < indiv_total_tasks_cnts.size(); ++i) indiv_total_tasks_cnts[i] = 0;
      for (size_t i = 0; i < task_switches.size(); ++i) task_switches[i] = 0;
    }
  };


protected:
  // == Configurable experiment parameters ==
  size_t RUN_MODE;
  int RANDOM_SEED;
  size_t POP_SIZE;
  size_t GENERATIONS;
  size_t EVAL_TIME;
  size_t TRIAL_CNT;
  std::string ANCESTOR_FPATH;
  double TASK_BASE_REWARD;
  double TASK_SWITCHING_PENALTY;
  size_t INDIV_TASK_CAP;
  size_t DEME_WIDTH;
  size_t DEME_HEIGHT;
  size_t PROPAGULE_SIZE;
  bool PROPAGULE_CLUMPY;
  bool ANY_TIME_ACTIVATION;
  bool TAG_BASED_ACTIVATION;
  size_t INBOX_CAPACITY;
  size_t TOURNAMENT_SIZE;
  size_t SELECTION_METHOD;
  size_t ELITE_SELECT__ELITE_CNT;
  size_t SGP_PROG_MAX_FUNC_CNT;
  size_t SGP_PROG_MIN_FUNC_CNT;
  size_t SGP_PROG_MAX_FUNC_LEN;
  size_t SGP_PROG_MIN_FUNC_LEN;
  size_t SGP_PROG_MAX_TOTAL_LEN;
  bool SGP_HW_EVENT_DRIVEN;
  bool SGP_HW_FORK_ON_MSG;
  size_t SGP_HW_MAX_CORES;
  size_t SGP_HW_MAX_CALL_DEPTH;
  double SGP_HW_MIN_BIND_THRESH;
  size_t SGP__PROG_MAX_ARG_VAL;
  double SGP__PER_BIT__TAG_BFLIP_RATE;
  double SGP__PER_INST__SUB_RATE;
  double SGP__PER_INST__INS_RATE;
  double SGP__PER_INST__DEL_RATE;
  double SGP__PER_FUNC__SLIP_RATE;
  double SGP__PER_FUNC__FUNC_DUP_RATE;
  double SGP__PER_FUNC__FUNC_DEL_RATE;
  size_t SYSTEMATICS_INTERVAL;
  size_t FITNESS_INTERVAL;
  size_t POP_SNAPSHOT_INTERVAL;
  std::string DATA_DIRECTORY;

  size_t DEME_SIZE;

  emp::Ptr<emp::Random> random;
  emp::Ptr<world_t> world;

  emp::Ptr<inst_lib_t> inst_lib;
  emp::Ptr<event_lib_t> event_lib;
  emp::Ptr<DOLDeme> eval_deme;
  
  using inbox_t = std::deque<event_t>;
  emp::vector<inbox_t> inboxes;

  using taskset_t = TaskSet<std::array<task_io_t,MAX_TASK_NUM_INPUTS>,task_io_t>;
  taskset_t task_set;
  std::array<task_io_t, MAX_TASK_NUM_INPUTS> task_inputs; ///< Current task inputs.
  size_t input_load_id;

  size_t update;
  size_t eval_time;

  size_t dom_agent_id;
  tag_t propagule_start_tag;

  emp::vector<Phenotype> agent_phen_cache;

  // Run signals.
  emp::Signal<void(void)> do_begin_run_setup_sig;   ///< Triggered at begining of run. Shared between AGP and SGP
  emp::Signal<void(void)> do_pop_init_sig;          ///< Triggered during run setup. Defines way population is initialized.
  emp::Signal<void(void)> do_evaluation_sig;        ///< Triggered during run step. Should trigger population-wide agent evaluation.
  emp::Signal<void(void)> do_selection_sig;         ///< Triggered during run step. Should trigger selection (which includes selection, reproduction, and mutation).
  emp::Signal<void(void)> do_world_update_sig;      ///< Triggered during run step. Should trigger world->Update(), and whatever else should happen right before/after population turnover.
  emp::Signal<void(void)> do_analysis_sig;
  // Systematics signals.
  emp::Signal<void(size_t)> do_pop_snapshot_sig;    ///< Triggered if we should take a snapshot of the population (as defined by POP_SNAPSHOT_INTERVAL). Should call appropriate functions to take snapshot.
  // Agent signals.
  emp::Signal<void(Agent &)> begin_agent_eval_sig;
  // emp::Signal<void(Agent &)> record_cur_phenotype_sig;
  emp::Signal<void(size_t, const tag_t &, const memory_t &)> on_activate_sig; 

  void ResetInboxes() {
    for (size_t i = 0; i < inboxes.size(); ++i) inboxes[i].clear();
  }

  void ResetInbox(size_t id) {
    emp_assert(id < inboxes.size());
    inboxes[id].clear();
  }

  inbox_t & GetInbox(size_t id) {
    emp_assert(id < inboxes.size());
    return inboxes[id];
  }

  bool InboxFull(size_t id) const { 
    emp_assert(id < inboxes.size());
    return inboxes[id].size() >= INBOX_CAPACITY; 
  }

  bool InboxEmpty(size_t id) const {
    emp_assert(id < inboxes.size());
    return inboxes[id].empty();
  }

  // Deliver message (event) to specified inbox. 
  // Make room by clearing out old messages (back of deque). 
  void DeliverToInbox(size_t id, const event_t & event) {
    emp_assert(id < inboxes.size());
    while (InboxFull(id)) inboxes[id].pop_back();
    inboxes[id].emplace_front(event);
  }
  
  size_t GetCacheIndex(size_t agent_id, size_t trial_id) {
    return (agent_id * TRIAL_CNT) + trial_id;
  }

  void SubmitTask(size_t hw_id, size_t task_id) {
    // Submit and record task (TASK_ID) completion for HW_ID.
    // reward = (BASE * SWITCH_PENALTY)*(1/(2**DEME_TASK_N))
    Phenotype & phen = agent_phen_cache[eval_deme->GetPhenID()];
    const size_t prev_task_cnt = phen.GetIndivTotalTaskCnt(hw_id);
    if (prev_task_cnt < INDIV_TASK_CAP) {
      const int last_task_id = eval_deme->GetLastTask(hw_id);
      const bool task_switch = !(task_id == last_task_id || last_task_id == NO_TASK);
      const double switch_penalty = (task_switch) ? TASK_SWITCHING_PENALTY : 1;
      const double deme_task_cnt = phen.GetDemeTaskCnt(task_id);
      const double reward = (TASK_BASE_REWARD * switch_penalty)/(emp::Pow2(deme_task_cnt));

      // Update shit.
      phen.deme_tasks_cnts[task_id]++;
      phen.indiv_tasks_cnts[phen.IndivTaskIndex(hw_id, task_id)]++;
      phen.indiv_total_tasks_cnts[hw_id]++;
      phen.task_switches[hw_id] += (size_t)task_switch;
      phen.total_task_switches += (size_t)task_switch;
      phen.task_total++;
      phen.score += reward;

      eval_deme->SetLastTask(hw_id, task_id);
    }
  }

  /// Guarantee no solution collisions!
  void ResetTasks() {
    task_inputs[0] = random->GetUInt(MIN_TASK_INPUT, MAX_TASK_INPUT);
    task_inputs[1] = random->GetUInt(MIN_TASK_INPUT, MAX_TASK_INPUT);
    task_set.SetInputs(task_inputs);
    while (task_set.IsCollision()) {
      task_inputs[0] = random->GetUInt(MIN_TASK_INPUT, MAX_TASK_INPUT);
      task_inputs[1] = random->GetUInt(MIN_TASK_INPUT, MAX_TASK_INPUT);
      task_set.SetInputs(task_inputs);
    }
  }

  void Evaluate(Agent & agent) {
    begin_agent_eval_sig.Trigger(agent);
    for (eval_time = 0; eval_time < EVAL_TIME; ++eval_time) {
      eval_deme->SingleAdvance();
    }
    // Record everything we want to store about trial phenotype:
    // record_cur_phenotype_sig.Trigger(agent); // TODO: might not need this!
  }

  /// Test function.
  /// Exists to test features as I add them.
  void Test() {
    std::cout << "Running tests!" << std::endl;
    // Test propragule activation.
    // 1) Load an ancestor program.
    do_pop_init_sig.Trigger();
    Agent & agent = world->GetOrg(0);
    std::cout << "---- TEST PROGRAM ----" << std::endl;
    agent.GetGenome().PrintProgramFull();
    std::cout << "----------------------" << std::endl;

    agent.SetID(0);
    eval_deme->SetProgram(agent.GetGenome());
    eval_deme->SetPhenID(0);
    agent_phen_cache[0].Reset();
    ResetTasks();
    std::cout << "Before begin-agent-eval signal!" << std::endl;
    eval_deme->PrintState();
    begin_agent_eval_sig.Trigger(agent);
    std::cout << "Post begin-agent-eval signal!" << std::endl;
    eval_deme->PrintActive();
    eval_deme->PrintState();
    std::cout << "------ RUNNING! ------" << std::endl;
    Phenotype & phen = agent_phen_cache[0];
    for (eval_time = 0; eval_time < EVAL_TIME; ++eval_time) {
      eval_deme->SingleAdvance();
      
      std::cout << "=========================== TIME: " << eval_time << " ===========================" << std::endl;
      
      eval_deme->PrintActive();
      
      // Print inbox sizes
      std::cout << "Inbox cnts: [";
      for (size_t i = 0; i < inboxes.size(); ++i) {
        std::cout << " " << i << ":" << inboxes[i].size();
      } std::cout << "]" << std::endl;
      
      // Print Phenotype info
      std::cout << "PHENOTYPE INFORMATION" << std::endl;
      std::cout << "Score: " << phen.score << std::endl;
      std::cout << "Task total: " << phen.task_total << std::endl;    
      std::cout << "Task switch totals: " << phen.total_task_switches << std::endl;
      std::cout << "Deme task cnts: [";
      for (size_t i = 0; i < task_set.GetSize(); ++i) {
        std::cout << " " << task_set.GetName(i) << ":" << phen.GetDemeTaskCnt(i);
      } std::cout << "]" << std::endl;
      std::cout << "Individual informations: " << std::endl;
      for (size_t hwID = 0; hwID < eval_deme->GetSize(); ++hwID) {
        std::cout << " -- " << hwID << " -- " << std::endl;
        std::cout << "  Total tasks: " << phen.GetIndivTotalTaskCnt(hwID) << std::endl;
        std::cout << "  Task switches: " << phen.GetIndivTaskSwitches(hwID) << std::endl;
        std::cout << "  Task cnts: [";
        for (size_t i = 0; i < task_set.GetSize(); ++i) {
          std::cout << " " << task_set.GetName(i) << ":" << phen.GetIndivTaskCnt(hwID, i);
        } std::cout << "]" << std::endl;
      }
      eval_deme->PrintState();
    }
    std::cout << "DONE EVALUATING DEME" << std::endl;


    // Print Phenotype info
    std::cout << "PHENOTYPE INFORMATION" << std::endl;
    std::cout << "Score: " << phen.score << std::endl;
    std::cout << "Task total: " << phen.task_total << std::endl;    
    std::cout << "Task switch totals: " << phen.total_task_switches << std::endl;
    std::cout << "Deme task cnts: [";
    for (size_t i = 0; i < task_set.GetSize(); ++i) {
      std::cout << " " << task_set.GetName(i) << ":" << phen.GetDemeTaskCnt(i);
    } std::cout << "]" << std::endl;
    std::cout << "Individual informations: " << std::endl;
    for (size_t hwID = 0; hwID < eval_deme->GetSize(); ++hwID) {
      std::cout << " -- " << hwID << " -- " << std::endl;
      std::cout << "  Total tasks: " << phen.GetIndivTotalTaskCnt(hwID) << std::endl;
      std::cout << "  Task switches: " << phen.GetIndivTaskSwitches(hwID) << std::endl;
      std::cout << "  Task cnts: [";
      for (size_t i = 0; i < task_set.GetSize(); ++i) {
        std::cout << " " << task_set.GetName(i) << ":" << phen.GetIndivTaskCnt(hwID, i);
      } std::cout << "]" << std::endl;
    }
    std::cout << "Mutual information: \n" << phen.CalcMutInfo() << std::endl;

    // Test tasks.
    // size_t trials_with_collisions = 0;
    // bool collision = false;
    // for (size_t trials = 0; trials < 10000; ++trials) {
    //   std::cout << "=========================== TRAIL: " << trials << " ===========================" << std::endl;
    //   ResetTasks();
    //   std::cout << "Check collisions: " << task_set.IsCollision() << std::endl;
    //   std::unordered_set<task_io_t> solutions;
    //   emp::vector<std::string> collisions;
    //   for (size_t i = 0; i < task_set.GetSize(); ++i) {
    //     taskset_t::Task & task_i = task_set.GetTask(i);
    //     for (size_t k = i+1; k < task_set.GetSize(); ++k) {
    //       taskset_t::Task & task_k = task_set.GetTask(k);
    //       std::cout << "=== Comparing Task " << task_i.name << " and Task " << task_k.name << " ===" << std::endl;
    //       // Compare solutions.
    //       for (size_t s_i = 0; s_i < task_i.solutions.size(); ++s_i) {
    //         for (size_t s_k = 0; s_k < task_k.solutions.size(); ++s_k) {
    //           task_io_t i_sol = task_i.solutions[s_i];
    //           task_io_t k_sol = task_k.solutions[s_k];
    //           if (i_sol == k_sol) {
    //             collision = true;
    //             std::cout << "Task " << task_i.name << " shares solution with Task " << task_k.name << std::endl;
    //             std::cout << "  " << i_sol << " == " << k_sol << std::endl;
    //             std::cout << "  Inputs: " << task_inputs[0] << "," << task_inputs[1] << std::endl;
    //           }
    //         }
    //       }
    //     }
    //   }
    //   if (collision) trials_with_collisions++;
    //   collision = false;
    // }
    // std::cout << "TOTAL TRIALS WITH COLLISIONS: " << trials_with_collisions << std::endl;
    exit(-1);
  }

public:
  Experiment(const DOLConfig & config)
    : DEME_SIZE(0), inboxes(0),
      input_load_id(0), update(0),
      eval_time(0), dom_agent_id(0), propagule_start_tag()
  {
    RUN_MODE = config.RUN_MODE();
    RANDOM_SEED = config.RANDOM_SEED();
    POP_SIZE = config.POP_SIZE();
    GENERATIONS = config.GENERATIONS();
    EVAL_TIME = config.EVAL_TIME();
    TRIAL_CNT = config.TRIAL_CNT();
    TASK_BASE_REWARD = config.TASK_BASE_REWARD();
    TASK_SWITCHING_PENALTY = config.TASK_SWITCHING_PENALTY();
    INDIV_TASK_CAP = config.INDIV_TASK_CAP();
    DEME_WIDTH = config.DEME_WIDTH();
    DEME_HEIGHT = config.DEME_HEIGHT();
    PROPAGULE_SIZE = config.PROPAGULE_SIZE();
    PROPAGULE_CLUMPY = config.PROPAGULE_CLUMPY();
    ANY_TIME_ACTIVATION = config.ANY_TIME_ACTIVATION();
    TAG_BASED_ACTIVATION = config.TAG_BASED_ACTIVATION();
    INBOX_CAPACITY = config.INBOX_CAPACITY();
    ANCESTOR_FPATH = config.ANCESTOR_FPATH();
    TOURNAMENT_SIZE = config.TOURNAMENT_SIZE();
    SELECTION_METHOD = config.SELECTION_METHOD();
    ELITE_SELECT__ELITE_CNT = config.ELITE_SELECT__ELITE_CNT();
    SGP_PROG_MAX_FUNC_CNT = config.SGP_PROG_MAX_FUNC_CNT();
    SGP_PROG_MIN_FUNC_CNT = config.SGP_PROG_MIN_FUNC_CNT();
    SGP_PROG_MAX_FUNC_LEN = config.SGP_PROG_MAX_FUNC_LEN();
    SGP_PROG_MIN_FUNC_LEN = config.SGP_PROG_MIN_FUNC_LEN();
    SGP_PROG_MAX_TOTAL_LEN = config.SGP_PROG_MAX_TOTAL_LEN();
    SGP_HW_EVENT_DRIVEN = config.SGP_HW_EVENT_DRIVEN();
    SGP_HW_FORK_ON_MSG = config.SGP_HW_FORK_ON_MSG();
    SGP_HW_MAX_CORES = config.SGP_HW_MAX_CORES();
    SGP_HW_MAX_CALL_DEPTH = config.SGP_HW_MAX_CALL_DEPTH();
    SGP_HW_MIN_BIND_THRESH = config.SGP_HW_MIN_BIND_THRESH();
    SGP__PROG_MAX_ARG_VAL = config.SGP__PROG_MAX_ARG_VAL();
    SGP__PER_BIT__TAG_BFLIP_RATE = config.SGP__PER_BIT__TAG_BFLIP_RATE();
    SGP__PER_INST__SUB_RATE = config.SGP__PER_INST__SUB_RATE();
    SGP__PER_INST__INS_RATE = config.SGP__PER_INST__INS_RATE();
    SGP__PER_INST__DEL_RATE = config.SGP__PER_INST__DEL_RATE();
    SGP__PER_FUNC__SLIP_RATE = config.SGP__PER_FUNC__SLIP_RATE();
    SGP__PER_FUNC__FUNC_DUP_RATE = config.SGP__PER_FUNC__FUNC_DUP_RATE();
    SGP__PER_FUNC__FUNC_DEL_RATE = config.SGP__PER_FUNC__FUNC_DEL_RATE();
    SYSTEMATICS_INTERVAL = config.SYSTEMATICS_INTERVAL();
    FITNESS_INTERVAL = config.FITNESS_INTERVAL();
    POP_SNAPSHOT_INTERVAL = config.POP_SNAPSHOT_INTERVAL();
    DATA_DIRECTORY = config.DATA_DIRECTORY();

    DEME_SIZE = DEME_WIDTH*DEME_HEIGHT;

    // Make the random number generator.
    random = emp::NewPtr<emp::Random>(RANDOM_SEED);

    // Make the world!
    world = emp::NewPtr<world_t>(random, "World");

    // Build phenotype cache.
    agent_phen_cache.resize(POP_SIZE);
    for (size_t i = 0; i < agent_phen_cache.size(); ++i) {
      Phenotype & phen = agent_phen_cache[i];
      phen.deme_tasks_cnts.resize(TASK_CNT);
      phen.indiv_tasks_cnts.resize(DEME_SIZE*TASK_CNT);
      phen.indiv_total_tasks_cnts.resize(DEME_SIZE);
      phen.task_switches.resize(DEME_SIZE);
      phen.Reset();
    }

    // Make inst/event libraries.
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();
    // Propagule start tag will be all 0s
    propagule_start_tag.Clear();

    // Configure tasks.
    Config_Tasks();

    // Configure hardware/instruction libs.
    Config_HW();

    // Configure given run mode.
    switch (RUN_MODE) {
      case RUN_ID__EXP:
        Config_Run();
        break;
      case RUN_ID__ANALYSIS:
        Config_Analysis();
        break;
    }
    // Test();
  }

  void Run() {
    switch (RUN_MODE) {
      case RUN_ID__EXP:
        do_begin_run_setup_sig.Trigger();
        for (update = 0; update <= GENERATIONS; ++update) {
          RunStep();
          if (update % POP_SNAPSHOT_INTERVAL == 0) do_pop_snapshot_sig.Trigger(update);
        }
        break;
      case RUN_ID__ANALYSIS:
        std::cout << "Analysis mode not implemented yet..." << std::endl;
        exit(-1);
        do_analysis_sig.Trigger();
        break;
      default:
        std::cout << "Unrecognized run mode! Exiting..." << std::endl;
        exit(-1);
    }
  }

  void RunStep() {
    do_evaluation_sig.Trigger();
    do_selection_sig.Trigger();
    do_world_update_sig.Trigger();
  }

  void Config_Tasks();
  void Config_HW();
  void Config_Run();
  void Config_Analysis();

  size_t Mutate(Agent & agent, emp::Random & rnd);
  double CalcFitness(Agent & agent) { return agent_phen_cache[agent.GetID()].GetScore(); } ;

  void InitPopulation_FromAncestorFile();
  void Snapshot_SingleFile(size_t update);

  emp::DataFile & AddDominantFile(const std::string & fpath="dominant.csv");

  // Instructions
  // (execution control)
  static void Inst_Fork(hardware_t & hw, const inst_t & inst);
  static void Inst_Terminate(hardware_t & hw, const inst_t & inst);
  // (logic tasks)
  static void Inst_Nand(hardware_t & hw, const inst_t & inst);
  void Inst_Load1(hardware_t & hw, const inst_t & inst);
  void Inst_Load2(hardware_t & hw, const inst_t & inst);
  void Inst_Submit(hardware_t & hw, const inst_t & inst);
  // (deme)
  //   - 'Reproduction' equivalent
  void Inst_ActivateFacing(hardware_t & hw, const inst_t & inst);
  //   - Orientation
  static void Inst_RotCW(hardware_t & hw, const inst_t & inst);
  static void Inst_RotCCW(hardware_t & hw, const inst_t & inst);
  static void Inst_GetDir(hardware_t & hw, const inst_t & inst);
  //   - Messaging
  static void Inst_SendMsgFacing(hardware_t & hw, const inst_t & inst);
  static void Inst_BroadcastMsg(hardware_t & hw, const inst_t & inst);
  void Inst_RetrieveMsg(hardware_t & hw, const inst_t & inst);
  //   - Roles
  static void Inst_GetRoleID(hardware_t & hw, const inst_t & inst);
  static void Inst_SetRoleID(hardware_t & hw, const inst_t & inst);
  //   - Location
  void Inst_GetLocXY(hardware_t & hw, const inst_t & inst);
  
  // Events
  void EventDriven__DispatchMessage_Send(hardware_t & hw, const event_t & event);
  void EventDriven__DispatchMessage_Broadcast(hardware_t & hw, const event_t & event);
  void Imperative__DispatchMessage_Send(hardware_t & hw, const event_t & event);
  void Imperative__DispatchMessage_Broadcast(hardware_t & hw, const event_t & event);
  static void HandleEvent__Message_Forking(hardware_t & hw, const event_t & event);
  static void HandleEvent__Message_NonForking(hardware_t & hw, const event_t & event);
};

// --- Instruction implementations ---
/// Instruction: Fork
/// Description: Fork thread with local memory as new thread's input buffer.
void Experiment::Inst_Fork(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  hw.SpawnCore(inst.affinity, hw.GetMinBindThresh(), state.local_mem);
}

void Experiment::Inst_Nand(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  const task_io_t a = (task_io_t)state.GetLocal(inst.args[0]);
  const task_io_t b = (task_io_t)state.GetLocal(inst.args[1]);
  state.SetLocal(inst.args[2], ~(a&b));
}

void Experiment::Inst_Terminate(hardware_t & hw, const inst_t & inst) {
  // Pop all the call states from current core.
  exec_stk_t & core = hw.GetCurCore();
  core.resize(0);
}

void Experiment::Inst_Load1(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  state.SetLocal(inst.args[0], task_inputs[input_load_id]); // Load input.
  input_load_id += 1;
  if (input_load_id >= task_inputs.size()) input_load_id = 0; // Update load ID.
}

void Experiment::Inst_Load2(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  state.SetLocal(inst.args[0], task_inputs[0]);
  state.SetLocal(inst.args[1], task_inputs[1]);
}

void Experiment::Inst_Submit(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  // Submit! --> Did hw complete a task?
  task_io_t sol = (task_io_t)state.GetLocal(inst.args[0]);
  size_t hw_id = hw.GetTrait(TRAIT_ID__DEME_ID);
  // NOTE: Task solutions are guaranteed to be unique to each task.
  for (size_t task_id = 0; task_id < task_set.GetSize(); ++task_id) {
    if (task_set.CheckTask(task_id, sol)) {
      SubmitTask(hw_id, task_id);
      break;
    }
  }
}

void Experiment::Inst_ActivateFacing(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  const size_t loc_id = (size_t)hw.GetTrait(TRAIT_ID__DEME_ID);
  const size_t dir = (size_t)hw.GetTrait(TRAIT_ID__DIR);
  const size_t facing_id = eval_deme->GetNeighborID(loc_id, dir);
  on_activate_sig.Trigger(facing_id, inst.affinity, state.output_mem);
}

void Experiment::Inst_RotCW(hardware_t & hw, const inst_t & inst) {
  hw.SetTrait(TRAIT_ID__DIR, emp::Mod(hw.GetTrait(TRAIT_ID__DIR) - 1, DOLDeme::NUM_DIRS));
}

void Experiment::Inst_RotCCW(hardware_t & hw, const inst_t & inst) {
  hw.SetTrait(TRAIT_ID__DIR, emp::Mod(hw.GetTrait(TRAIT_ID__DIR) + 1, DOLDeme::NUM_DIRS));
}

void Experiment::Inst_GetDir(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  state.SetLocal(inst.args[0], hw.GetTrait(TRAIT_ID__DIR));
}

void Experiment::Inst_SendMsgFacing(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  hw.TriggerEvent("SendMessage", inst.affinity, state.output_mem);
}

void Experiment::Inst_BroadcastMsg(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  hw.TriggerEvent("BroadcastMessage", inst.affinity, state.output_mem);
}

void Experiment::Inst_RetrieveMsg(hardware_t & hw, const inst_t & inst) {
  const size_t loc_id = (size_t)hw.GetTrait(TRAIT_ID__DEME_ID);
  std::cout << "Inst: RetrieveMsg!" << std::endl;
  if (!InboxEmpty(loc_id)) {
    std::cout << "Inbox is not empty!" << std::endl;
    inbox_t & inbox = GetInbox(loc_id);
    hw.HandleEvent(inbox.front());
    inbox.pop_front(); // Remove!
  }
}

void Experiment::Inst_GetRoleID(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  state.SetLocal(inst.args[0], hw.GetTrait(TRAIT_ID__ROLE_ID));
}

void Experiment::Inst_SetRoleID(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  hw.SetTrait(TRAIT_ID__ROLE_ID, state.GetLocal(inst.args[0]));
}

void Experiment::Inst_GetLocXY(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  const size_t x = eval_deme->GetLocX((size_t)hw.GetTrait(TRAIT_ID__DEME_ID));
  const size_t y = eval_deme->GetLocY((size_t)hw.GetTrait(TRAIT_ID__DEME_ID));
  state.SetLocal(inst.args[0], x);
  state.SetLocal(inst.args[1], y);
}

void Experiment::EventDriven__DispatchMessage_Send(hardware_t & hw, const event_t & event) {
  const size_t facing_id = eval_deme->GetNeighborID((size_t)hw.GetTrait(TRAIT_ID__DEME_ID), (size_t)hw.GetTrait(TRAIT_ID__DIR));
  hardware_t & rHW = eval_deme->GetHardware(facing_id);
  if (eval_deme->IsActive(facing_id)) rHW.QueueEvent(event);
}

void Experiment::EventDriven__DispatchMessage_Broadcast(hardware_t & hw, const event_t & event) {
  const size_t loc_id = (size_t)hw.GetTrait(TRAIT_ID__DEME_ID);
  const size_t uid = eval_deme->GetNeighborID(loc_id, DOLDeme::DIR_UP);
  const size_t did = eval_deme->GetNeighborID(loc_id, DOLDeme::DIR_DOWN);
  const size_t lid = eval_deme->GetNeighborID(loc_id, DOLDeme::DIR_LEFT);
  const size_t rid = eval_deme->GetNeighborID(loc_id, DOLDeme::DIR_RIGHT);
  if (eval_deme->IsActive(uid)) eval_deme->GetHardware(uid).QueueEvent(event);  
  if (eval_deme->IsActive(did)) eval_deme->GetHardware(did).QueueEvent(event);
  if (eval_deme->IsActive(lid)) eval_deme->GetHardware(lid).QueueEvent(event);
  if (eval_deme->IsActive(rid)) eval_deme->GetHardware(rid).QueueEvent(event);  
}

void Experiment::Imperative__DispatchMessage_Send(hardware_t & hw, const event_t & event) {
  const size_t facing_id = eval_deme->GetNeighborID(hw.GetTrait(TRAIT_ID__DEME_ID), hw.GetTrait(TRAIT_ID__DIR));
  if (eval_deme->IsActive(facing_id)) DeliverToInbox(facing_id, event);
}

void Experiment::Imperative__DispatchMessage_Broadcast(hardware_t & hw, const event_t & event) {
  const size_t loc_id = (size_t)hw.GetTrait(TRAIT_ID__DEME_ID);
  const size_t uid = eval_deme->GetNeighborID(loc_id, DOLDeme::DIR_UP);
  const size_t did = eval_deme->GetNeighborID(loc_id, DOLDeme::DIR_DOWN);
  const size_t lid = eval_deme->GetNeighborID(loc_id, DOLDeme::DIR_LEFT);
  const size_t rid = eval_deme->GetNeighborID(loc_id, DOLDeme::DIR_RIGHT);
  if (eval_deme->IsActive(uid)) DeliverToInbox(uid, event);
  if (eval_deme->IsActive(did)) DeliverToInbox(did, event);
  if (eval_deme->IsActive(lid)) DeliverToInbox(lid, event);
  if (eval_deme->IsActive(rid)) DeliverToInbox(rid, event);
}

void Experiment::HandleEvent__Message_Forking(hardware_t & hw, const event_t & event) {
  // Spawn a new core.
  hw.SpawnCore(event.affinity, hw.GetMinBindThresh(), event.msg);
}

void Experiment::HandleEvent__Message_NonForking(hardware_t & hw, const event_t & event) {
  // Instead of spawning a new core, load event data into input buffer of current call state.
  state_t & state = hw.GetCurState();
  // Loop through event memory... 
  for (auto mem : event.msg) { state.SetInput(mem.first, mem.second); }
}

// --- Utilities ---
void Experiment::InitPopulation_FromAncestorFile() {
  std::cout << "Initializing population from ancestor file!" << std::endl;
  // Configure the ancestor program.
  program_t ancestor_prog(inst_lib);
  std::ifstream ancestor_fstream(ANCESTOR_FPATH);
  if (!ancestor_fstream.is_open()) {
    std::cout << "Failed to open ancestor program file(" << ANCESTOR_FPATH << "). Exiting..." << std::endl;
    exit(-1);
  }
  ancestor_prog.Load(ancestor_fstream);
  std::cout << " --- Ancestor program: ---" << std::endl;
  ancestor_prog.PrintProgramFull();
  std::cout << " -------------------------" << std::endl;
  world->Inject(ancestor_prog, 1);    // Inject a bunch of ancestors into the population.
}

void Experiment::Snapshot_SingleFile(size_t update) {
  std::string snapshot_dir = DATA_DIRECTORY + "pop_" + emp::to_string((int)update);
  mkdir(snapshot_dir.c_str(), ACCESSPERMS);
  // For each program in the population, dump the full program description in a single file.
  std::ofstream prog_ofstream(snapshot_dir + "/pop_" + emp::to_string((int)update) + ".pop");
  for (size_t i = 0; i < world->GetSize(); ++i) {
    if (i) prog_ofstream << "===\n";
    Agent & agent = world->GetOrg(i);
    agent.program.PrintProgramFull(prog_ofstream);
  }
  prog_ofstream.close();
}

emp::DataFile & Experiment::AddDominantFile(const std::string & fpath) {
  auto & file = world->SetupFile(fpath);

  std::function<size_t(void)> get_update = [this](){ return world->GetUpdate(); };
  file.AddFun(get_update, "update", "Update");

  std::function<double(void)> get_score = [this]() {
    Phenotype & phen = agent_phen_cache[dom_agent_id];
    return phen.GetScore();
  };
  file.AddFun(get_score, "score", "Dominant score");

  std::function<double(void)> get_mut_info = [this]() {
    Phenotype & phen = agent_phen_cache[dom_agent_id];
    return phen.CalcMutInfo();
  };
  file.AddFun(get_mut_info, "mutual_information", "Shannon mutual information (division of labor)");

  /*
  indiv_tasks_cnts
  indiv_total_tasks_cnts
  task_switches
  */
  std::function<double(void)> get_deme_task_total = [this]() {
    Phenotype & phen = agent_phen_cache[dom_agent_id];
    return phen.GetDemeTotalTaskCnt();
  };
  file.AddFun(get_deme_task_total, "deme_total_tasks", "Total count of tasks done by deme");

  std::function<double(void)> get_deme_switch_total = [this]() {
    Phenotype & phen = agent_phen_cache[dom_agent_id];
    return phen.GetDemeTaskSwitches();
  };
  file.AddFun(get_deme_switch_total, "deme_total_switches", "Total count of task switches done by deme");

  // Deme totals by task. 
  for (size_t taskID = 0; taskID < task_set.GetSize(); ++taskID) {
    std::function<double(void)> get_task_total = [this, taskID]() {
      Phenotype & phen = agent_phen_cache[dom_agent_id];
      return phen.GetDemeTaskCnt(taskID);
    };
    file.AddFun(get_task_total, "deme_"+task_set.GetName(taskID)+"_total", "Total count of specified task.");
  }
  file.PrintHeaderKeys();
  return file;

}

// --- Configuration/setup function implementations ---
void Experiment::Config_Run() {
  // Make data directory.
  mkdir(DATA_DIRECTORY.c_str(), ACCESSPERMS);
  if (DATA_DIRECTORY.back() != '/') DATA_DIRECTORY += '/';
  
  // Configure the world.
  world->Reset();
  world->SetWellMixed(true);
  world->SetFitFun([this](Agent & agent) { return this->CalcFitness(agent); });
  world->SetMutFun([this](Agent &agent, emp::Random &rnd) { return this->Mutate(agent, rnd); }, ELITE_SELECT__ELITE_CNT);

  // === Setup signals! ===
  // On population initialization:
  do_pop_init_sig.AddAction([this]() {
    this->InitPopulation_FromAncestorFile();
  });

  // On run setup:
  do_begin_run_setup_sig.AddAction([this]() {
    std::cout << "Doing initial run setup." << std::endl;
    // Setup systematics/fitness tracking.
    auto & sys_file = world->SetupSystematicsFile(DATA_DIRECTORY + "systematics.csv");
    sys_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
    auto & fit_file = world->SetupFitnessFile(DATA_DIRECTORY + "fitness.csv");
    fit_file.SetTimingRepeat(FITNESS_INTERVAL);
    this->AddDominantFile(DATA_DIRECTORY+"dominant.csv").SetTimingRepeat(SYSTEMATICS_INTERVAL);
    do_pop_init_sig.Trigger();
  });

  begin_agent_eval_sig.AddAction([this](Agent & agent) {
    // Do agent setup at the beginning of its evaluation.
    // - Here, the eval_deme has been reset.
    // - No agents in deme have active threads.
    // Setup propagule.
    eval_deme->ActivateDemePropagule(PROPAGULE_SIZE, PROPAGULE_CLUMPY);
  });

  // On evaluation:
  do_evaluation_sig.AddAction([this]() {
    double best_score = -32767;
    dom_agent_id = 0;
    ResetTasks();
    for (size_t id = 0; id < world->GetSize(); ++id) {
      Agent & our_hero = world->GetOrg(id);
      our_hero.SetID(id);
      eval_deme->SetProgram(our_hero.GetGenome());
      eval_deme->SetPhenID(id);
      agent_phen_cache[id].Reset();
      this->Evaluate(our_hero);
      if (agent_phen_cache[id].GetScore() > best_score) { best_score = agent_phen_cache[id].GetScore(); dom_agent_id = id; }
    }
    std::cout << "Update: " << update << " Max score: " << best_score << std::endl;
  });
  
  do_selection_sig.AddAction([this]() {
    emp::EliteSelect(*world, ELITE_SELECT__ELITE_CNT, 1);
    emp::TournamentSelect(*world, TOURNAMENT_SIZE, POP_SIZE - ELITE_SELECT__ELITE_CNT);
  });
  
  // Do world update action
  do_world_update_sig.AddAction([this]() {
    world->Update();
  });

  // Do population snapshot action
  do_pop_snapshot_sig.AddAction([this](size_t update) { this->Snapshot_SingleFile(update); }); 

}

void Experiment::Config_Analysis() { ; }

void Experiment::Config_HW() {
  // - Setup the instruction set. -
  // Standard instructions:
  inst_lib->AddInst("Inc", hardware_t::Inst_Inc, 1, "Increment value in local memory Arg1");
  inst_lib->AddInst("Dec", hardware_t::Inst_Dec, 1, "Decrement value in local memory Arg1");
  inst_lib->AddInst("Not", hardware_t::Inst_Not, 1, "Logically toggle value in local memory Arg1");
  inst_lib->AddInst("Add", hardware_t::Inst_Add, 3, "Local memory: Arg3 = Arg1 + Arg2");
  inst_lib->AddInst("Sub", hardware_t::Inst_Sub, 3, "Local memory: Arg3 = Arg1 - Arg2");
  inst_lib->AddInst("Mult", hardware_t::Inst_Mult, 3, "Local memory: Arg3 = Arg1 * Arg2");
  inst_lib->AddInst("Div", hardware_t::Inst_Div, 3, "Local memory: Arg3 = Arg1 / Arg2");
  inst_lib->AddInst("Mod", hardware_t::Inst_Mod, 3, "Local memory: Arg3 = Arg1 % Arg2");
  inst_lib->AddInst("TestEqu", hardware_t::Inst_TestEqu, 3, "Local memory: Arg3 = (Arg1 == Arg2)");
  inst_lib->AddInst("TestNEqu", hardware_t::Inst_TestNEqu, 3, "Local memory: Arg3 = (Arg1 != Arg2)");
  inst_lib->AddInst("TestLess", hardware_t::Inst_TestLess, 3, "Local memory: Arg3 = (Arg1 < Arg2)");
  inst_lib->AddInst("If", hardware_t::Inst_If, 1, "Local memory: If Arg1 != 0, proceed; else, skip block.", emp::ScopeType::BASIC, 0, {"block_def"});
  inst_lib->AddInst("While", hardware_t::Inst_While, 1, "Local memory: If Arg1 != 0, loop; else, skip block.", emp::ScopeType::BASIC, 0, {"block_def"});
  inst_lib->AddInst("Countdown", hardware_t::Inst_Countdown, 1, "Local memory: Countdown Arg1 to zero.", emp::ScopeType::BASIC, 0, {"block_def"});
  inst_lib->AddInst("Close", hardware_t::Inst_Close, 0, "Close current block if there is a block to close.", emp::ScopeType::BASIC, 0, {"block_close"});
  inst_lib->AddInst("Break", hardware_t::Inst_Break, 0, "Break out of current block.");
  inst_lib->AddInst("Call", hardware_t::Inst_Call, 0, "Call function that best matches call affinity.", emp::ScopeType::BASIC, 0, {"affinity"});
  inst_lib->AddInst("Return", hardware_t::Inst_Return, 0, "Return from current function if possible.");
  inst_lib->AddInst("SetMem", hardware_t::Inst_SetMem, 2, "Local memory: Arg1 = numerical value of Arg2");
  inst_lib->AddInst("CopyMem", hardware_t::Inst_CopyMem, 2, "Local memory: Arg1 = Arg2");
  inst_lib->AddInst("SwapMem", hardware_t::Inst_SwapMem, 2, "Local memory: Swap values of Arg1 and Arg2.");
  inst_lib->AddInst("Input", hardware_t::Inst_Input, 2, "Input memory Arg1 => Local memory Arg2.");
  inst_lib->AddInst("Output", hardware_t::Inst_Output, 2, "Local memory Arg1 => Output memory Arg2.");
  inst_lib->AddInst("Commit", hardware_t::Inst_Commit, 2, "Local memory Arg1 => Shared memory Arg2.");
  inst_lib->AddInst("Pull", hardware_t::Inst_Pull, 2, "Shared memory Arg1 => Shared memory Arg2.");
  inst_lib->AddInst("Nop", hardware_t::Inst_Nop, 0, "No operation.");
  inst_lib->AddInst("Fork", Inst_Fork, 0, "Fork a new thread. Local memory contents of callee are loaded into forked thread's input memory.");
  inst_lib->AddInst("Nand", Inst_Nand, 3, "WM[ARG3]=~(WM[ARG1]&WM[ARG2])");
  inst_lib->AddInst("Terminate", Inst_Terminate, 0, "Kill current thread.");

  // Add experiment-specific instructions.
  inst_lib->AddInst("Load-1", [this](hardware_t & hw, const inst_t & inst) { this->Inst_Load1(hw, inst); }, 1, "WM[ARG1] = TaskInput[LOAD_ID]; LOAD_ID++;");
  inst_lib->AddInst("Load-2", [this](hardware_t & hw, const inst_t & inst) { this->Inst_Load2(hw, inst); }, 2, "WM[ARG1] = TASKINPUT[0]; WM[ARG2] = TASKINPUT[1];");
  inst_lib->AddInst("Submit", [this](hardware_t & hw, const inst_t & inst) { this->Inst_Submit(hw, inst); }, 1, "Submit WM[ARG1] as potential task solution.");
  // --> Broadcast, SendMsg, ActivateFacing
  inst_lib->AddInst("ActivateFacing", [this](hardware_t & hw, const inst_t & inst) { this->Inst_ActivateFacing(hw, inst); }, 0, "Activate faced neighbor.");
  // - Orientation instructions
  inst_lib->AddInst("RotCW", Inst_RotCW, 0, "Rotate clockwise");
  inst_lib->AddInst("RotCCW", Inst_RotCCW, 0, "Rotate couter-clockwise");
  inst_lib->AddInst("GetDir", Inst_GetDir, 0, "WM[ARG1]=CURRENT DIRECTION");
  // - Role instructions
  inst_lib->AddInst("GetRoleID", Inst_GetRoleID, 1, "WM[ARG1]=TRAITS[ROLE_ID]");
  inst_lib->AddInst("SetRoleID", Inst_SetRoleID, 1, "TRAITS[ROLE_ID]=WM[ARG1]");
  // - Location instructions
  inst_lib->AddInst("GetLocXY", [this](hardware_t & hw, const inst_t & inst) { this->Inst_GetLocXY(hw, inst); }, 2, "WM[ARG1]=LOCX, WM[ARG2]=LOCY");
  // - Messaging instructions
  inst_lib->AddInst("SendMsg", Inst_SendMsgFacing, 0, "Send output memory as message event to faced neighbor.", emp::ScopeType::BASIC, 0, {"affinity"});
  inst_lib->AddInst("BroadcastMsg", Inst_BroadcastMsg, 0, "Broadcast output memory as message event.", emp::ScopeType::BASIC, 0, {"affinity"});

  // Configure evaluation hardware.
  // Make eval deme.
  eval_deme = emp::NewPtr<DOLDeme>(DEME_WIDTH, DEME_HEIGHT, random, inst_lib, event_lib);
  eval_deme->SetHardwareMinBindThresh(SGP_HW_MIN_BIND_THRESH);
  eval_deme->SetHardwareMaxCores(SGP_HW_MAX_CORES);
  eval_deme->SetHardwareMaxCallDepth(SGP_HW_MAX_CALL_DEPTH);

  eval_deme->OnHardwareReset([this](hardware_t & hw) {
    hw.SetTrait(TRAIT_ID__ACTIVE, 0);
    hw.SetTrait(TRAIT_ID__LAST_TASK, NO_TASK);
    hw.SetTrait(TRAIT_ID__UID, 0);
    hw.SetTrait(TRAIT_ID__DIR, 0);
    hw.SetTrait(TRAIT_ID__ROLE_ID, 0);
  });

  eval_deme->OnHardwareAdvance([this](hardware_t & hw) {
    if ((bool)hw.GetTrait(TRAIT_ID__ACTIVE)) hw.SingleProcess();
  });

  eval_deme->OnPropaguleActivation([this](hardware_t & hw) {
    // Trigger on_activate_sig
    on_activate_sig.Trigger(hw.GetTrait(TRAIT_ID__DEME_ID), propagule_start_tag, memory_t());
  });

  if (SGP_HW_FORK_ON_MSG) {
    event_lib->AddEvent("SendMessage", HandleEvent__Message_Forking, "Send message event.");
    event_lib->AddEvent("BroadcastMessage", HandleEvent__Message_Forking, "Broadcast message event.");
  } else {
    event_lib->AddEvent("SendMessage", HandleEvent__Message_NonForking, "Send message event.");
    event_lib->AddEvent("BroadcastMessage", HandleEvent__Message_NonForking, "Broadcast message event.");
  }

  if (SGP_HW_EVENT_DRIVEN) { // Hardware is event-driven.
    
    // Configure dispatchers
    event_lib->RegisterDispatchFun("SendMessage", [this](hardware_t & hw, const event_t & event) {
      this->EventDriven__DispatchMessage_Send(hw, event);

    });
    event_lib->RegisterDispatchFun("BroadcastMessage", [this](hardware_t &hw, const event_t &event) {
      this->EventDriven__DispatchMessage_Broadcast(hw, event);
    });
  } else { // Hardware is imperative.
    
    // Add retrieve message instruction to instruction set.
    inst_lib->AddInst("RetrieveMsg", [this](hardware_t & hw, const inst_t & inst) {
        this->Inst_RetrieveMsg(hw, inst);
      }, 0, "Retrieve a message from message inbox.");
    
    // Configure dispatchers
    event_lib->RegisterDispatchFun("SendMessage", [this](hardware_t & hw, const event_t & event) {
      this->Imperative__DispatchMessage_Send(hw, event);
    });
    event_lib->RegisterDispatchFun("BroadcastMessage", [this](hardware_t &hw, const event_t &event) {
      this->Imperative__DispatchMessage_Broadcast(hw, event);
    });
   
    // Configure inboxes.
    inboxes.resize(DEME_SIZE);
    eval_deme->OnHardwareReset([this](hardware_t & hw) {
      this->ResetInbox(hw.GetTrait(TRAIT_ID__DEME_ID));
    });
  }

  // What happens on activate signal?
  if (TAG_BASED_ACTIVATION && ANY_TIME_ACTIVATION) {
    // Tag-based, anytime activation.
    on_activate_sig.AddAction([this](size_t activate_id, const tag_t & activate_tag, const memory_t & in_mem) {
      // Tell eval deme that this agent is active. 
      eval_deme->Activate(activate_id);
      // Give agent something to do... 
      hardware_t & hw = eval_deme->GetHardware(activate_id);
      hw.SpawnCore(activate_tag, 0.0, in_mem, false);
    });
  } else if (TAG_BASED_ACTIVATION && !ANY_TIME_ACTIVATION) {
    // Tag-based, 1-time activation.
    on_activate_sig.AddAction([this](size_t activate_id, const tag_t & activate_tag, const memory_t & in_mem) {
      if (!eval_deme->IsActive(activate_id)) {
        // Tell eval deme that this agent is active. 
        eval_deme->Activate(activate_id);
        // Give agent something to do... 
        hardware_t & hw = eval_deme->GetHardware(activate_id);
        hw.SpawnCore(activate_tag, 0.0, in_mem, false);
      }
    });
  } else if (!TAG_BASED_ACTIVATION && ANY_TIME_ACTIVATION) {
    // Not tag-based, any-time activation.
    on_activate_sig.AddAction([this](size_t activate_id, const tag_t & activate_tag, const memory_t & in_mem) {
      // Tell eval deme that this agent is active. 
      eval_deme->Activate(activate_id);
      // Give agent something to do... 
      hardware_t & hw = eval_deme->GetHardware(activate_id);
      hw.SpawnCore(0, in_mem, false);
    });
  } else if (!TAG_BASED_ACTIVATION && !ANY_TIME_ACTIVATION) {
    // Not tag-based, 1-time activation.
    on_activate_sig.AddAction([this](size_t activate_id, const tag_t &activate_tag, const memory_t &in_mem) {
      if (!eval_deme->IsActive(activate_id)) {      
        // Tell eval deme that this agent is active.
        eval_deme->Activate(activate_id);
        // Give agent something to do...
        hardware_t &hw = eval_deme->GetHardware(activate_id);
        hw.SpawnCore(0, in_mem, false);
      }
    });
  } else {
    // Should never get here!
    std::cout << "Not sure what you want me to do on activate signal!" << std::endl;
    exit(-1);
  }
}

void Experiment::Config_Tasks() {
  // Zero out task inputs.
  for (size_t i = 0; i < MAX_TASK_NUM_INPUTS; ++i) task_inputs[i] = 0;
  // Add tasks to task set.
  // NAND
  task_set.AddTask("NAND", [this](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(~(a&b));
  }, "NAND task");
  // NOT
  task_set.AddTask("NOT", [this](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(~a);
    task.solutions.emplace_back(~b);
  }, "NOT task");
  // ORN
  task_set.AddTask("ORN", [this](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back((a|(~b)));
    task.solutions.emplace_back((b|(~a)));
  }, "ORN task");
  // AND
  task_set.AddTask("AND", [this](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(a&b);
  }, "AND task");
  // OR
  task_set.AddTask("OR", [this](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(a|b);
  }, "OR task");
  // ANDN
  task_set.AddTask("ANDN", [this](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back((a&(~b)));
    task.solutions.emplace_back((b&(~a)));
  }, "ANDN task");
  // NOR
  task_set.AddTask("NOR", [this](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(~(a|b));
  }, "NOR task");
  // XOR
  task_set.AddTask("XOR", [this](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(a^b);
  }, "XOR task");
  // EQU
  task_set.AddTask("EQU", [this](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(~(a^b));
  }, "EQU task");
}

/// Mutation rules:
/// - Function Duplication (per-program rate):
///   - Result cannot allow program to exceed max function count
///   - Result cannot allow program to exceed max total instruction length.
/// - Function Deletion (per-program rate).
///   - Result cannot allow program to have no functions.
/// - Slip mutations (per-function rate)
///   - Result cannot allow function length to break the range [PROG_MIN_FUNC_LEN:PROG_MAX_FUNC_LEN]
/// - Instruction insertion/deletion mutations (per-instruction rate)
///   - Result cannot allow function length to break [PROG_MIN_FUNC_LEN:PROG_MAX_FUNC_LEN]
///   - Result cannot allow function length to exeed PROG_MAX_TOTAL_LEN
size_t Experiment::Mutate(Agent &agent, emp::Random &rnd) {
  program_t &program = agent.GetGenome();
  size_t mut_cnt = 0;
  size_t expected_prog_len = program.GetInstCnt();

  // Duplicate a (single) function?
  if (rnd.P(SGP__PER_FUNC__FUNC_DUP_RATE) && program.GetSize() < SGP_PROG_MAX_FUNC_CNT)
  {
    const uint32_t fID = rnd.GetUInt(program.GetSize());
    // Would function duplication make expected program length exceed max?
    if (expected_prog_len + program[fID].GetSize() <= SGP_PROG_MAX_TOTAL_LEN)
    {
      program.PushFunction(program[fID]);
      expected_prog_len += program[fID].GetSize();
      ++mut_cnt;
    }
  }

  // Delete a (single) function?
  if (rnd.P(SGP__PER_FUNC__FUNC_DEL_RATE) && program.GetSize() > SGP_PROG_MIN_FUNC_CNT)
  {
    const uint32_t fID = rnd.GetUInt(program.GetSize());
    expected_prog_len -= program[fID].GetSize();
    program[fID] = program[program.GetSize() - 1];
    program.program.resize(program.GetSize() - 1);
    ++mut_cnt;
  }

  // For each function...
  for (size_t fID = 0; fID < program.GetSize(); ++fID)
  {

    // Mutate affinity
    for (size_t i = 0; i < program[fID].GetAffinity().GetSize(); ++i)
    {
      tag_t &aff = program[fID].GetAffinity();
      if (rnd.P(SGP__PER_BIT__TAG_BFLIP_RATE))
      {
        ++mut_cnt;
        aff.Set(i, !aff.Get(i));
      }
    }

    // Slip-mutation?
    if (rnd.P(SGP__PER_FUNC__SLIP_RATE))
    {
      uint32_t begin = rnd.GetUInt(program[fID].GetSize());
      uint32_t end = rnd.GetUInt(program[fID].GetSize());
      const bool dup = begin < end;
      const bool del = begin > end;
      const int dup_size = end - begin;
      const int del_size = begin - end;
      // If we would be duplicating and the result will not exceed maximum program length, duplicate!
      if (dup && (expected_prog_len + dup_size <= SGP_PROG_MAX_TOTAL_LEN) && (program[fID].GetSize() + dup_size <= SGP_PROG_MAX_FUNC_LEN))
      {
        // duplicate begin:end
        const size_t new_size = program[fID].GetSize() + (size_t)dup_size;
        hardware_t::Function new_fun(program[fID].GetAffinity());
        for (size_t i = 0; i < new_size; ++i)
        {
          if (i < end)
            new_fun.PushInst(program[fID][i]);
          else
            new_fun.PushInst(program[fID][i - dup_size]);
        }
        program[fID] = new_fun;
        ++mut_cnt;
        expected_prog_len += dup_size;
      }
      else if (del && ((program[fID].GetSize() - del_size) >= SGP_PROG_MIN_FUNC_LEN))
      {
        // delete end:begin
        hardware_t::Function new_fun(program[fID].GetAffinity());
        for (size_t i = 0; i < end; ++i)
          new_fun.PushInst(program[fID][i]);
        for (size_t i = begin; i < program[fID].GetSize(); ++i)
          new_fun.PushInst(program[fID][i]);
        program[fID] = new_fun;
        ++mut_cnt;
        expected_prog_len -= del_size;
      }
    }

    // Substitution mutations? (pretty much completely safe)
    for (size_t i = 0; i < program[fID].GetSize(); ++i)
    {
      inst_t &inst = program[fID][i];
      // Mutate affinity (even when it doesn't use it).
      for (size_t k = 0; k < inst.affinity.GetSize(); ++k)
      {
        if (rnd.P(SGP__PER_BIT__TAG_BFLIP_RATE))
        {
          ++mut_cnt;
          inst.affinity.Set(k, !inst.affinity.Get(k));
        }
      }

      // Mutate instruction.
      if (rnd.P(SGP__PER_INST__SUB_RATE))
      {
        ++mut_cnt;
        inst.id = rnd.GetUInt(program.GetInstLib()->GetSize());
      }

      // Mutate arguments (even if they aren't relevent to instruction).
      for (size_t k = 0; k < hardware_t::MAX_INST_ARGS; ++k)
      {
        if (rnd.P(SGP__PER_INST__SUB_RATE))
        {
          ++mut_cnt;
          inst.args[k] = rnd.GetInt(SGP__PROG_MAX_ARG_VAL);
        }
      }
    }

    // Insertion/deletion mutations?
    // - Compute number of insertions.
    int num_ins = rnd.GetRandBinomial(program[fID].GetSize(), SGP__PER_INST__INS_RATE);
    // Ensure that insertions don't exceed maximum program length.
    if ((num_ins + program[fID].GetSize()) > SGP_PROG_MAX_FUNC_LEN)
    {
      num_ins = SGP_PROG_MAX_FUNC_LEN - program[fID].GetSize();
    }
    if ((num_ins + expected_prog_len) > SGP_PROG_MAX_TOTAL_LEN)
    {
      num_ins = SGP_PROG_MAX_TOTAL_LEN - expected_prog_len;
    }
    expected_prog_len += num_ins;

    // Do we need to do any insertions or deletions?
    if (num_ins > 0 || SGP__PER_INST__DEL_RATE > 0.0)
    {
      size_t expected_func_len = num_ins + program[fID].GetSize();
      // Compute insertion locations and sort them.
      emp::vector<size_t> ins_locs = emp::RandomUIntVector(rnd, num_ins, 0, program[fID].GetSize());
      if (ins_locs.size())
        std::sort(ins_locs.begin(), ins_locs.end(), std::greater<size_t>());
      hardware_t::Function new_fun(program[fID].GetAffinity());
      size_t rhead = 0;
      while (rhead < program[fID].GetSize())
      {
        if (ins_locs.size())
        {
          if (rhead >= ins_locs.back())
          {
            // Insert a random instruction.
            new_fun.PushInst(rnd.GetUInt(program.GetInstLib()->GetSize()),
                             rnd.GetInt(SGP__PROG_MAX_ARG_VAL),
                             rnd.GetInt(SGP__PROG_MAX_ARG_VAL),
                             rnd.GetInt(SGP__PROG_MAX_ARG_VAL),
                             tag_t());
            new_fun.inst_seq.back().affinity.Randomize(rnd);
            ++mut_cnt;
            ins_locs.pop_back();
            continue;
          }
        }
        // Do we delete this instruction?
        if (rnd.P(SGP__PER_INST__DEL_RATE) && (expected_func_len > SGP_PROG_MIN_FUNC_LEN))
        {
          ++mut_cnt;
          --expected_prog_len;
          --expected_func_len;
        }
        else
        {
          new_fun.PushInst(program[fID][rhead]);
        }
        ++rhead;
      }
      program[fID] = new_fun;
    }
  }
  return mut_cnt;
}

#endif
