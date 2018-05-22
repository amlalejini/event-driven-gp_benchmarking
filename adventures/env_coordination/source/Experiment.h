#ifndef ENVCOORD_EXPERIMENT_H
#define ENVCOORD_EXPERIMENT_H

// @includes
#include <iostream>
#include <string>
#include <utility>
#include <fstream>
#include <sys/stat.h>
#include <algorithm>
#include <functional>

#include "base/Ptr.h"
#include "base/vector.h"
#include "control/Signal.h"
#include "Evolve/World.h"
#include "hardware/EventDrivenGP.h"
#include "hardware/InstLib.h"
#include "tools/BitVector.h"
#include "tools/Random.h"
#include "tools/random_utils.h"
#include "tools/math.h"
#include "tools/string_utils.h"
#include "tools/stats.h"

#include "../../utility_belt/source/utilities.h"

#include "l9_chg_env-config.h"
#include "TaskSet.h"

constexpr size_t TAG_WIDTH = 16;

constexpr uint32_t MIN_TASK_INPUT = 0;
constexpr uint32_t MAX_TASK_INPUT = 1000000000;
constexpr size_t MAX_TASK_NUM_INPUTS = 2;

constexpr size_t RUN_ID__EVO = 0;
constexpr size_t RUN_ID__MAPE = 1;
constexpr size_t RUN_ID__ANALYSIS = 2;

constexpr size_t ENV_TAG_GEN_ID__RANDOM = 0;
constexpr size_t ENV_TAG_GEN_ID__LOAD = 1;

constexpr size_t TRAIT_ID__STATE = 0;

class Experiment {
public:
  // Forward declarations.
  struct Agent;
  struct Phenotype;
  class PhenotypeCache;

  // Type aliases
  // - Hardware aliases
  using hardware_t = emp::EventDrivenGP_AW<TAG_WIDTH>;
  using state_t = hardware_t::State;
  using program_t = hardware_t::Program;
  using function_t = hardware_t::Function;
  using inst_t = hardware_t::inst_t;
  using inst_lib_t = hardware_t::inst_lib_t;
  using event_t = hardware_t::event_t;
  using event_lib_t = hardware_t::event_lib_t;
  using memory_t = hardware_t::memory_t;
  using tag_t = hardware_t::affinity_t;
  using exec_stk_t = hardware_t::exec_stk_t;
  // - Agent aliases
  using agent_t = Agent;
  using phenotype_t = Phenotype;
  using phen_cache_t = PhenotypeCache;
  // - World aliases
  using world_t = emp::World<agent_t>;
  using task_io_t = uint32_t;
  using taskset_t = TaskSet<std::array<task_io_t, MAX_TASK_NUM_INPUTS>, task_io_t>;


  /// Agent to be evolved.
  struct Agent {
    size_t agent_id;
    program_t program;
    double sim_thresh;

    Agent(const program_t & _p) : agent_id(0), program(_p) { ; }
    Agent(const Agent && in) : agent_id(in.GetID()), program(in.program) { ; }
    Agent(const Agent & in): agent_id(in.GetID()), program(in.program) { ; }

    size_t GetID() const { return agent_id; }
    void SetID(size_t id) { agent_id = id; }

    double GetSimilarityThreshold() const { return sim_thresh; }
    void SetSimilarityThreshold(double val) { sim_thresh = val; }

    program_t & GetGenome() { return program; }
  };

  /// Phenotype of agents being evolved.
  struct Phenotype {
    double env_match_score;
    size_t functions_used;
    double inst_entropy;
    double sim_thresh;

    double score;

    size_t task_cnt;
    size_t time_all_tasks_credited;
    size_t total_wasted_completions;
    size_t unique_tasks_credited;
    size_t unique_tasks_completed;

    emp::vector<size_t> wasted_completions_by_task;
    emp::vector<size_t> credited_by_task;
    emp::vector<size_t> completed_by_task;


    Phenotype(size_t _task_cnt=0) 
      : env_match_score(0),
        functions_used(0),
        inst_entropy(0),
        sim_thresh(0),
        score(0),
        task_cnt(_task_cnt),
        time_all_tasks_credited(0),
        total_wasted_completions(0),
        unique_tasks_credited(0),
        unique_tasks_completed(0),
        wasted_completions_by_task(task_cnt),
        credited_by_task(task_cnt),
        completed_by_task(task_cnt)
    { ; }

    /// Zero out phenotype.
    void Reset() {
      env_match_score = 0;
      functions_used = 0;
      sim_thresh = 0;
      score = 0;
      time_all_tasks_credited = 0;
      total_wasted_completions = 0;
      unique_tasks_credited = 0;
      unique_tasks_completed = 0;
      inst_entropy = 0;
      for (size_t i = 0; i < task_cnt; ++i) {
        wasted_completions_by_task[i] = 0;
        credited_by_task[i] = 0;
        completed_by_task[i] = 0;
      }
    }

    void SetTaskCnt(size_t val) {
      task_cnt = val;
      wasted_completions_by_task.resize(task_cnt);
      credited_by_task.resize(task_cnt);
      completed_by_task.resize(task_cnt);
      Reset();
    }

    double GetEnvMatchScore() const { return env_match_score; }
    size_t GetFunctionsUsed() const { return functions_used; }
    double GetInstEntropy() const { return inst_entropy; }
    double GetSimilarityThreshold() const { return sim_thresh; }
    double GetScore() const { return score; }
    size_t GetTaskCnt() const { return task_cnt; }
    size_t GetTimeAllTasksCredited() const { return time_all_tasks_credited; }
    size_t GetTotalWastedCompletions() const { return total_wasted_completions; }
    size_t GetUniqueTasksCredited() const { return unique_tasks_credited; }
    size_t GetUniqueTasksCompleted() const { return unique_tasks_completed; }
    size_t GetWastedCompletions(size_t task_id) const { 
      emp_assert(task_id < wasted_completions_by_task.size());
      return wasted_completions_by_task[task_id]; 
    }
    size_t GetCredited(size_t task_id) const { 
      emp_assert(task_id < credited_by_task.size());
      return credited_by_task[task_id]; 
    }
    size_t GetCompleted(size_t task_id) const { 
      emp_assert(task_id < completed_by_task.size());
      return completed_by_task[task_id]; 
    }

    void SetEnvMatchScore(double val) { env_match_score = val; }
    void SetFunctionsUsed(size_t val) { functions_used = val; }
    void SetInstEntropy(double val) { inst_entropy = val; }
    void SetSimilarityThreshold(double val) { sim_thresh = val; }
    void SetScore(double val) { score = val; }

    void SetTimeAllTasksCredited(size_t val) { time_all_tasks_credited = val; }
    void SetTotalWastedCompletions(size_t val) { total_wasted_completions = val; }
    void SetUniqueTasksCredited(size_t val) { unique_tasks_credited = val; }
    void SetUniqueTasksCompleted(size_t val) { unique_tasks_completed = val; }
    
    void SetWastedCompletions(size_t task_id, size_t val) { 
      emp_assert(task_id < wasted_completions_by_task.size());
      wasted_completions_by_task[task_id] = val; 
    }
    void SetCredited(size_t task_id, size_t val) { 
      emp_assert(task_id < credited_by_task.size());
      credited_by_task[task_id] = val; 
    }
    void SetCompleted(size_t task_id, size_t val) { 
      emp_assert(task_id < completed_by_task.size());
      completed_by_task[task_id] = val; 
    }

  };

  /// Utility class used to cache phenotypes during population evaluation.
  class PhenotypeCache {
    protected:
      size_t agent_cnt;
      size_t eval_cnt;
      emp::vector<phenotype_t> agent_phen_cache;
      emp::vector<size_t> agent_representative_eval;

    public:
      PhenotypeCache(size_t _agent_cnt, size_t _eval_cnt) 
        : agent_cnt(_agent_cnt), eval_cnt(_eval_cnt), 
          agent_phen_cache(agent_cnt * eval_cnt),
          agent_representative_eval(agent_cnt, 0)
      { ; }

      /// Resize phenotype cache. 
      void Resize(size_t _agent_cnt, size_t _eval_cnt) {
        agent_cnt = _agent_cnt;
        eval_cnt = _eval_cnt;
        agent_phen_cache.clear();
        agent_phen_cache.resize(agent_cnt * eval_cnt);
        agent_representative_eval.clear();
        agent_representative_eval.resize(agent_cnt, 0);
      }

      /// Access a phenotype from the cache
      phenotype_t & Get(size_t agent_id, size_t eval_id) {
        return agent_phen_cache[(agent_id * eval_cnt) + eval_id];
      }

      size_t GetRepresentativeEval(size_t agent_id) {
        emp_assert(agent_id < agent_cnt);
        return agent_representative_eval[agent_id];
      }

      phenotype_t & GetRepresentativePhen(size_t agent_id) {
        return Get(agent_id, agent_representative_eval[agent_id]);
      }

      /// Set representative evaluation to worst-scoring evaluation.
      void SetRepresentativeEval(size_t agent_id) {
        emp_assert(agent_id < agent_cnt);
        // agent_representative_eval[agent_id] = eval_id;
        double score = Get(agent_id, 0).GetScore();
        size_t repID = 0;
        // Return the minimum score!
        for (size_t eID = 1; eID < eval_cnt; ++eID) {
          phenotype_t & phen = Get(agent_id, eID);
          if (phen.GetScore() < score) { score = phen.GetScore(); repID = eID; }
        }
        agent_representative_eval[agent_id] = repID;
      }    
  };

protected:
  // Configurable parameters
  // == DEFAULT_GROUP ==
  size_t RUN_MODE; 
  int RANDOM_SEED; 
  size_t POP_SIZE; 
  size_t GENERATIONS; 
  size_t POP_INIT_METHOD; 
  std::string ANCESTOR_FPATH; 
  // == EVALUATION_GROUP == 
  size_t EVAL_TIME; 
  size_t TRIAL_CNT; 
  bool TASKS_ON; 
  // == ENVIRONMENT_GROUP ==
  size_t ENVIRONMENT_STATES; 
  size_t ENVIRONMENT_TAG_GENERATION_METHOD; 
  std::string ENVIRONMENT_TAG_FPATH; 
  size_t ENVIRONMENT_CHANGE_METHOD; 
  double ENVIRONMENT_CHANGE_PROB; 
  size_t ENVIRONMENT_CHANGE_INTERVAL; 
  size_t ENVIRONMENT_DISTRACTION_SIGNALS; 
  double ENVIRONMENT_DISTRACTION_SIGNAL_PROB;
  // == SELECTION_GROUP ==
  size_t TOURNAMENT_SIZE; 
  size_t SELECTION_METHOD; 
  size_t ELITE_SELECT__ELITE_CNT; 
  bool MAP_ELITES_AXIS__INST_ENTROPY; 
  bool MAP_ELITES_AXIS__FUNCTIONS_USED; 
  bool MAP_ELITES_AXIS__SIMILARITY_THRESH; 
  double MAP_ELITES_AXIS_RES__INST_ENTROPY; 
  double MAP_ELITES_AXIS_RES__SIMILARITY_THRESH; 
  // == SGP_PROGRAM_GROUP ==
  size_t SGP_PROG_MAX_FUNC_CNT; 
  size_t SGP_PROG_MIN_FUNC_CNT; 
  size_t SGP_PROG_MAX_FUNC_LEN; 
  size_t SGP_PROG_MIN_FUNC_LEN; 
  size_t SGP_PROG_MAX_TOTAL_LEN; 
  // == SGP_HARDWARE_GROUP ==  
  bool SGP_ENVIRONMENT_SIGNALS; 
  bool SGP_ACTIVE_SENSORS; 
  size_t SGP_HW_MAX_CORES; 
  size_t SGP_HW_MAX_CALL_DEPTH; 
  double SGP_HW_MIN_BIND_THRESH; 
  // == SGP_MUTATION_GROUP ==
  int SGP_MUT_PROG_MAX_ARG_VAL; 
  double SGP_MUT_PER_BIT__TAG_BFLIP_RATE; 
  double SGP_MUT_PER_INST__SUB_RATE; 
  double SGP_MUT_PER_INST__INS_RATE; 
  double SGP_MUT_PER_INST__DEL_RATE; 
  double SGP_MUT_PER_FUNC__SLIP_RATE; 
  double SGP_MUT_PER_FUNC__FUNC_DUP_RATE; 
  double SGP_MUT_PER_FUNC__FUNC_DEL_RATE; 
  // == DATA_GROUP ==
  size_t SYSTEMATICS_INTERVAL; 
  size_t FITNESS_INTERVAL; 
  size_t POP_SNAPSHOT_INTERVAL; 
  std::string DATA_DIRECTORY; 
  // == ANALYSIS_GROUP ==
  size_t ANALYSIS_METHOD; 
  std::string ANALYZE_AGENT_FPATH; 
  std::string ANALYSIS_OUTPUT_FNAME; 

  // Experiment variables
  emp::Ptr<emp::Random> random; ///< Random number generator
  emp::Ptr<world_t> world;      ///< Empirical world for evolution

  emp::Ptr<inst_lib_t> inst_lib;    ///< SignalGP instruction library
  emp::Ptr<event_lib_t> event_lib;  ///< SignalGP event library

  emp::Ptr<hardware_t> eval_hw;     ///< SignalGP virtual hardware used for evaluation

  emp::vector<tag_t> env_state_tags;        ///< Tags associated with each environment state.
  emp::vector<tag_t> distraction_sig_tags;  ///< Tags associated with distraction signals.

  taskset_t task_set;
  std::array<task_io_t, MAX_TASK_NUM_INPUTS> task_inputs;
  size_t input_load_id;

  size_t update;
  size_t trial_id;
  size_t trial_time;
  size_t env_state;

  size_t dom_agent_id;
  double best_score;

  double max_inst_entropy;
  std::unordered_set<size_t> functions_used;

  phen_cache_t phen_cache;

  // Run signals
  emp::Signal<void(void)> do_begin_run_setup_sig;   ///< Triggered at begining of run.
  emp::Signal<void(void)> do_pop_init_sig;          ///< Triggered during run setup. Defines way population is initialized.
  
  emp::Signal<void(void)> do_evaluation_sig;        ///< Triggered during run step. Should trigger population-wide agent evaluation.
  emp::Signal<void(void)> do_selection_sig;         ///< Triggered during run step. Should trigger selection (which includes selection, reproduction, and mutation).
  emp::Signal<void(void)> do_world_update_sig;      ///< Triggered during run step. Should trigger world->Update(), and whatever else should happen right before/after population turnover.
  emp::Signal<void(void)> do_analysis_sig;          ///< Triggered for analysis mode. 

  // Systematics signals
  emp::Signal<void(size_t)> do_pop_snapshot_sig;      ///< Triggered if we should take a snapshot of the population (as defined by POP_SNAPSHOT_INTERVAL). Should call appropriate functions to take snapshot.

  emp::Signal<void(agent_t &)> begin_agent_eval_sig;  ///< Triggered at beginning of agent evaluation (might be multiple trials)
  emp::Signal<void(agent_t &)> end_agent_eval_sig;  ///< Triggered at beginning of agent evaluation (might be multiple trials)
  
  emp::Signal<void(agent_t &)> begin_agent_trial_sig; ///< Triggered at the beginning of an agent trial.
  emp::Signal<void(agent_t &)> do_agent_trial_sig; ///< Triggered at the beginning of an agent trial.
  emp::Signal<void(agent_t &)> end_agent_trial_sig; ///< Triggered at the beginning of an agent trial.

  emp::Signal<void(agent_t &)> do_agent_advance_sig; ///< When triggered, advance SignalGP evaluation hardware

  // A few flexible functors!
  std::function<double(agent_t &)> calc_score;
  
  // For MAP-Elites
  std::function<double(agent_t &)> inst_ent_fun;
  std::function<int(agent_t &)> func_cnt_fun;

  /// Reset logic tasks, guaranteeing no solution collisions among the tasks.
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

  /// Evaluate given agent.
  void Evaluate(agent_t & agent) {
    begin_agent_eval_sig.Trigger(agent);
    for (trial_id = 0; trial_id < TRIAL_CNT; ++trial_id) {
      begin_agent_trial_sig.Trigger(agent);
      do_agent_trial_sig.Trigger(agent);
      end_agent_trial_sig.Trigger(agent);
    }
    end_agent_eval_sig.Trigger(agent);
  }

  /// Scratch/test function.
  void Test() {
    std::cout << "Testing experiment!" << std::endl;
  }


public:
  Experiment(const L9ChgEnvConfig & config)
    : input_load_id(0),
      update(0),
      trial_id(0),
      trial_time(0),
      env_state(0),
      dom_agent_id(0),
      best_score(0),
      max_inst_entropy(0),
      phen_cache(0,0)
  {
    // Localize configs!
    // == DEFAULT_GROUP ==
    RUN_MODE = config.RUN_MODE(); 
    RANDOM_SEED = config.RANDOM_SEED(); 
    POP_SIZE = config.POP_SIZE(); 
    GENERATIONS = config.GENERATIONS(); 
    POP_INIT_METHOD = config.POP_INIT_METHOD(); 
    ANCESTOR_FPATH = config.ANCESTOR_FPATH(); 
    // == EVALUATION_GROUP == 
    EVAL_TIME = config.EVAL_TIME(); 
    TRIAL_CNT = config.TRIAL_CNT(); 
    TASKS_ON = config.TASKS_ON(); 
    // == ENVIRONMENT_GROUP ==
    ENVIRONMENT_STATES = config.ENVIRONMENT_STATES(); 
    ENVIRONMENT_TAG_GENERATION_METHOD = config.ENVIRONMENT_TAG_GENERATION_METHOD(); 
    ENVIRONMENT_TAG_FPATH = config.ENVIRONMENT_TAG_FPATH(); 
    ENVIRONMENT_CHANGE_METHOD = config.ENVIRONMENT_CHANGE_METHOD(); 
    ENVIRONMENT_CHANGE_PROB = config.ENVIRONMENT_CHANGE_PROB(); 
    ENVIRONMENT_CHANGE_INTERVAL = config.ENVIRONMENT_CHANGE_INTERVAL(); 
    ENVIRONMENT_DISTRACTION_SIGNALS = config.ENVIRONMENT_DISTRACTION_SIGNALS(); 
    ENVIRONMENT_DISTRACTION_SIGNAL_PROB = config.ENVIRONMENT_DISTRACTION_SIGNAL_PROB();
    // == SELECTION_GROUP ==
    TOURNAMENT_SIZE = config.TOURNAMENT_SIZE(); 
    SELECTION_METHOD = config.SELECTION_METHOD(); 
    ELITE_SELECT__ELITE_CNT = config.ELITE_SELECT__ELITE_CNT(); 
    MAP_ELITES_AXIS__INST_ENTROPY = config.MAP_ELITES_AXIS__INST_ENTROPY(); 
    MAP_ELITES_AXIS__FUNCTIONS_USED = config.MAP_ELITES_AXIS__FUNCTIONS_USED(); 
    MAP_ELITES_AXIS__SIMILARITY_THRESH = config.MAP_ELITES_AXIS__SIMILARITY_THRESH(); 
    MAP_ELITES_AXIS_RES__INST_ENTROPY = config.MAP_ELITES_AXIS_RES__INST_ENTROPY(); 
    MAP_ELITES_AXIS_RES__SIMILARITY_THRESH = config.MAP_ELITES_AXIS_RES__SIMILARITY_THRESH(); 
    // == SGP_PROGRAM_GROUP ==
    SGP_PROG_MAX_FUNC_CNT = config.SGP_PROG_MAX_FUNC_CNT(); 
    SGP_PROG_MIN_FUNC_CNT = config.SGP_PROG_MIN_FUNC_CNT(); 
    SGP_PROG_MAX_FUNC_LEN = config.SGP_PROG_MAX_FUNC_LEN(); 
    SGP_PROG_MIN_FUNC_LEN = config.SGP_PROG_MIN_FUNC_LEN(); 
    SGP_PROG_MAX_TOTAL_LEN = config.SGP_PROG_MAX_TOTAL_LEN(); 
    // == SGP_HARDWARE_GROUP ==  
    SGP_ENVIRONMENT_SIGNALS = config.SGP_ENVIRONMENT_SIGNALS(); 
    SGP_ACTIVE_SENSORS = config.SGP_ACTIVE_SENSORS(); 
    SGP_HW_MAX_CORES = config.SGP_HW_MAX_CORES(); 
    SGP_HW_MAX_CALL_DEPTH = config.SGP_HW_MAX_CALL_DEPTH(); 
    SGP_HW_MIN_BIND_THRESH = config.SGP_HW_MIN_BIND_THRESH(); 
    // == SGP_MUTATION_GROUP ==
    SGP_MUT_PROG_MAX_ARG_VAL = config.SGP_MUT_PROG_MAX_ARG_VAL(); 
    SGP_MUT_PER_BIT__TAG_BFLIP_RATE = config.SGP_MUT_PER_BIT__TAG_BFLIP_RATE(); 
    SGP_MUT_PER_INST__SUB_RATE = config.SGP_MUT_PER_INST__SUB_RATE(); 
    SGP_MUT_PER_INST__INS_RATE = config.SGP_MUT_PER_INST__INS_RATE(); 
    SGP_MUT_PER_INST__DEL_RATE = config.SGP_MUT_PER_INST__DEL_RATE(); 
    SGP_MUT_PER_FUNC__SLIP_RATE = config.SGP_MUT_PER_FUNC__SLIP_RATE(); 
    SGP_MUT_PER_FUNC__FUNC_DUP_RATE = config.SGP_MUT_PER_FUNC__FUNC_DUP_RATE(); 
    SGP_MUT_PER_FUNC__FUNC_DEL_RATE = config.SGP_MUT_PER_FUNC__FUNC_DEL_RATE(); 
    // == DATA_GROUP ==
    SYSTEMATICS_INTERVAL = config.SYSTEMATICS_INTERVAL(); 
    FITNESS_INTERVAL = config.FITNESS_INTERVAL(); 
    POP_SNAPSHOT_INTERVAL = config.POP_SNAPSHOT_INTERVAL(); 
    DATA_DIRECTORY = config.DATA_DIRECTORY(); 
    // == ANALYSIS_GROUP ==
    ANALYSIS_METHOD = config.ANALYSIS_METHOD(); 
    ANALYZE_AGENT_FPATH = config.ANALYZE_AGENT_FPATH(); 
    ANALYSIS_OUTPUT_FNAME = config.ANALYSIS_OUTPUT_FNAME(); 

    // Create a new random number generator
    random = emp::NewPtr<emp::Random>(RANDOM_SEED);

    // Make the world!
    world = emp::NewPtr<world_t>(*random, "World");

    // Configure the phenotype cache.
    phen_cache.Resize(POP_SIZE, TRIAL_CNT);

    if (TRIAL_CNT < 1) {
      std::cout << "Cannot run experiment with TRIAL_CNT < 1. Exiting..." << std::endl;
      exit(-1);
    }

    // Configure the environment tags.
    switch(ENVIRONMENT_TAG_GENERATION_METHOD) {
      case ENV_TAG_GEN_ID__RANDOM: {
        env_state_tags = toolbelt::GenerateRandomTags<TAG_WIDTH>(*random, ENVIRONMENT_STATES, true);
        if (ENVIRONMENT_DISTRACTION_SIGNALS) distraction_sig_tags = toolbelt::GenerateRandomTags<TAG_WIDTH>(*random, ENVIRONMENT_DISTRACTION_SIGNALS, env_state_tags, true);
        SaveEnvTags();
        break;
      }
      case ENV_TAG_GEN_ID__LOAD: {
        GenerateEnvTags__FromTagFile();
        break;
      }
      default: {
        std::cout << "Unrecognized environment tag generation method (" << ENVIRONMENT_TAG_GENERATION_METHOD << "). Exiting..." << std::endl;
        exit(-1);
      }
    }
    // Print environment tags
    std::cout << "Environment tags (" << env_state_tags.size() << "): " << std::endl;
    for (size_t i = 0; i < env_state_tags.size(); ++i) {
      std::cout << i << ":";
      env_state_tags[i].Print();
      std::cout << std::endl;
    }
    std::cout << "Distraction signal tags (" << distraction_sig_tags.size() << "): " << std::endl;
    for (size_t i = 0; i < env_state_tags.size(); ++i) {
      std::cout << i << ":";
      distraction_sig_tags[i].Print();
      std::cout << std::endl;
    }

    // Make empty instruction/event libraries. Make hardware.
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();
    eval_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);

    
    // Configure hardware, etc
    DoConfig__Tasks();
    DoConfig__Hardware();

    switch (RUN_MODE) {
      case RUN_ID__EVO: {
        DoConfig__Evolution();
        DoConfig__Experiment();
        break;
      }
      case RUN_ID__MAPE: {
        DoConfig__MAPElites();
        DoConfig__Experiment();
        break;
      }
      case RUN_ID__ANALYSIS: {
        DoConfig__Analysis();
        break;
      }
      default: {
        std::cout << "Unrecognized run mode (" << RUN_MODE << "). Exiting..." << std::endl;
        exit(-1);
      }
    }

    Test();
  }

  ~Experiment() {
    eval_hw.Delete();
    event_lib.Delete();
    inst_lib.Delete();
    world.Delete();
    random.Delete();
  }

  // === Run functions ===
  void Run();
  void RunStep();

  // === Config functions ===
  void DoConfig__Hardware();
  void DoConfig__Tasks();

  void DoConfig__Evolution();  ///< Setup evolutionary algorithm
  void DoConfig__MAPElites();  ///< Setup MAP-Elites algorithm
  void DoConfig__Experiment(); ///< Setup experiment
  void DoConfig__Analysis();   ///< Setup analysis

  // === Utility functions ===
  void SaveEnvTags();
  void GenerateEnvTags__FromTagFile();

  void InitPopulation__FromAncestorFile();

  // === Systematics Functions ===
  void Snapshot__Programs(size_t u); 

  // === Extra SignalGP instruction definitions ===
  // -- Execution control instructions --
  static void Inst_Fork(hardware_t & hw, const inst_t & inst);      
  static void Inst_Terminate(hardware_t & hw, const inst_t & inst); 
  static void Inst_Nand(hardware_t & hw, const inst_t & inst);

  void Inst_Load1(hardware_t & hw, const inst_t & inst);
  void Inst_Load2(hardware_t & hw, const inst_t & inst);
  void Inst_Submit(hardware_t & hw, const inst_t & inst);

};

// == Extra SignalGP instructions ==
void Experiment::Inst_Fork(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  hw.SpawnCore(inst.affinity, hw.GetMinBindThresh(), state.local_mem, false);
}

void Experiment::Inst_Terminate(hardware_t & hw, const inst_t & inst)  {
  // Pop all the call states from current core.
  exec_stk_t & core = hw.GetCurCore();
  core.resize(0);
}

void Experiment::Inst_Nand(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  const task_io_t a = (task_io_t)state.GetLocal(inst.args[0]);
  const task_io_t b = (task_io_t)state.GetLocal(inst.args[1]);
  state.SetLocal(inst.args[2], ~(a&b));
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
  // Credit?
  const bool credit = hw.GetTrait(TRAIT_ID__STATE) == env_state;
  // Submit!
  task_set.Submit((task_io_t)state.GetLocal(inst.args[0]), trial_time, credit);
}

// === Run functions ===
void Experiment::Run() {
  switch(RUN_MODE) {
    case RUN_ID__EVO:
    case RUN_ID__MAPE: {
      do_begin_run_setup_sig.Trigger();
      for (update = 0; update <= GENERATIONS; ++update) {
        RunStep();
      }
      break;
    }
    case RUN_ID__ANALYSIS: {
      do_analysis_sig.Trigger();
      break;
    }
    default: {
      std::cout << "Unrecognized run mode (" << RUN_MODE << "). Exiting..." << std::endl;
      exit(-1);
    }
  }
}

void Experiment::RunStep() {
  do_evaluation_sig.Trigger();
  do_selection_sig.Trigger();
  do_world_update_sig.Trigger();
}

// == utility functions ==
void Experiment::SaveEnvTags() {

}
void Experiment::GenerateEnvTags__FromTagFile() {

}
void Experiment::InitPopulation__FromAncestorFile() {

}

// == Systematics functions ==
void Experiment::Snapshot__Programs(size_t u) {

}

// == Configuration functions ==
void Experiment::DoConfig__Hardware() {

}
void Experiment::DoConfig__Tasks() {

}
void Experiment::DoConfig__Evolution() {

}
void Experiment::DoConfig__MAPElites() {

}
void Experiment::DoConfig__Experiment() {

}
void Experiment::DoConfig__Analysis() {

}

#endif
