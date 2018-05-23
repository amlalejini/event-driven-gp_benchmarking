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

constexpr double MAX_SIM_THRESH = 1.0;
constexpr double MIN_SIM_THRESH = 0.0;

constexpr size_t RUN_ID__EVO = 0;
constexpr size_t RUN_ID__MAPE = 1;
constexpr size_t RUN_ID__ANALYSIS = 2;

constexpr size_t ENV_TAG_GEN_ID__RANDOM = 0;
constexpr size_t ENV_TAG_GEN_ID__LOAD = 1;

constexpr size_t POP_INIT_METHOD_ID__ANCESTOR = 0;
constexpr size_t POP_INIT_METHOD_ID__RANDOM = 1;

constexpr size_t ENV_CHG_METHOD_ID__RANDOM = 0;
constexpr size_t ENV_CHG_METHOD_ID__REGULAR = 1;

constexpr size_t TRAIT_ID__STATE = 0;

constexpr size_t SELECTION_METHOD_ID__TOURNAMENT = 0;

constexpr double MIN_POSSIBLE_SCORE = -32767;

class Experiment {
public:
  // Forward declarations.
  struct Agent;
  struct Genome;
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
  using genome_t = Genome;
  // - World aliases
  using world_t = emp::World<agent_t>;
  using task_io_t = uint32_t;
  using taskset_t = TaskSet<std::array<task_io_t, MAX_TASK_NUM_INPUTS>, task_io_t>;

  struct Genome {
    program_t program;
    double sim_thresh;

    Genome(const program_t & _p, double _s=0) : program(_p), sim_thresh(_s) { ; }
    Genome(const Genome && in) : program(in.program), sim_thresh(in.sim_thresh) { ; }
    Genome(const Genome & in) : program(in.program), sim_thresh(in.sim_thresh) { ; } 

  };

  /// Agent to be evolved.
  struct Agent {
    size_t agent_id;
    genome_t genome;

    Agent(const program_t & _p, double _s=0) : agent_id(0), genome(_p, _s) { ; }
    Agent(const genome_t & _g) : agent_id(0), genome(_g) { ; }
    Agent(const Agent && in) : agent_id(in.GetID()), genome(in.genome) { ; }
    Agent(const Agent & in) : agent_id(in.GetID()), genome(in.genome) { ; }

    size_t GetID() const { return agent_id; }
    void SetID(size_t id) { agent_id = id; }

    double GetSimilarityThreshold() const { return genome.sim_thresh; }
    void SetSimilarityThreshold(double val) { genome.sim_thresh = val; }

    genome_t & GetGenome() { return genome; }
    program_t & GetProgram() { return genome.program; }

  };
  // TODO: reset phenotype on begin trial... 
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

    void IncEnvMatchScore(double val=1.0) { env_match_score += val; }

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
  bool EVOLVE_SIMILARITY_THRESH;
  // == ENVIRONMENT_GROUP ==
  size_t ENVIRONMENT_STATES; 
  size_t ENVIRONMENT_TAG_GENERATION_METHOD; 
  std::string ENVIRONMENT_TAG_FPATH; 
  size_t ENVIRONMENT_CHANGE_METHOD; 
  double ENVIRONMENT_CHANGE_PROB; 
  size_t ENVIRONMENT_CHANGE_INTERVAL; 
  bool ENVIRONMENT_DISTRACTION_SIGNALS; 
  size_t ENVIRONMENT_DISTRACTION_SIGNAL_CNT;
  double ENVIRONMENT_DISTRACTION_SIGNAL_PROB;
  // == SELECTION_GROUP ==
  size_t TOURNAMENT_SIZE; 
  size_t SELECTION_METHOD; 
  size_t ELITE_SELECT__ELITE_CNT; 
  bool MAP_ELITES_AXIS__INST_ENTROPY; 
  bool MAP_ELITES_AXIS__FUNCTIONS_USED; 
  bool MAP_ELITES_AXIS__SIMILARITY_THRESH; 
  size_t MAP_ELITES_AXIS_RES__INST_ENTROPY; 
  size_t MAP_ELITES_AXIS_RES__SIMILARITY_THRESH; 
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
  double SGP_MUT_PER_AGENT__SIM_THRESH_RATE;
  double SGP_MUT_PER_AGENT__SIM_THRESH_STD;
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
  size_t DOM_SNAPSHOT_TRIAL_CNT;
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

  toolbelt::SignalGPMutator<hardware_t> mutator;

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
  emp::Signal<void(void)> do_env_advance_sig;


  // A few flexible functors!
  std::function<double(agent_t &)> calc_score;
  
  // For MAP-Elites
  std::function<double(agent_t &)> inst_ent_fun;
  std::function<int(agent_t &)> func_cnt_fun;
  std::function<double(agent_t &)> get_sim_thresh_fun;

  std::function<size_t(agent_t &, emp::Random &)> mutate_agent;

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
    : mutator(),
      input_load_id(0),
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
    EVOLVE_SIMILARITY_THRESH = config.EVOLVE_SIMILARITY_THRESH();
    // == ENVIRONMENT_GROUP ==
    ENVIRONMENT_STATES = config.ENVIRONMENT_STATES(); 
    ENVIRONMENT_TAG_GENERATION_METHOD = config.ENVIRONMENT_TAG_GENERATION_METHOD(); 
    ENVIRONMENT_TAG_FPATH = config.ENVIRONMENT_TAG_FPATH(); 
    ENVIRONMENT_CHANGE_METHOD = config.ENVIRONMENT_CHANGE_METHOD(); 
    ENVIRONMENT_CHANGE_PROB = config.ENVIRONMENT_CHANGE_PROB(); 
    ENVIRONMENT_CHANGE_INTERVAL = config.ENVIRONMENT_CHANGE_INTERVAL(); 
    ENVIRONMENT_DISTRACTION_SIGNALS = config.ENVIRONMENT_DISTRACTION_SIGNALS(); 
    ENVIRONMENT_DISTRACTION_SIGNAL_CNT = config.ENVIRONMENT_DISTRACTION_SIGNAL_CNT();
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
    SGP_MUT_PER_AGENT__SIM_THRESH_RATE = config.SGP_MUT_PER_AGENT__SIM_THRESH_RATE();
    SGP_MUT_PER_AGENT__SIM_THRESH_STD = config.SGP_MUT_PER_AGENT__SIM_THRESH_STD();
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
    DOM_SNAPSHOT_TRIAL_CNT = config.DOM_SNAPSHOT_TRIAL_CNT();
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
        if (ENVIRONMENT_DISTRACTION_SIGNALS) distraction_sig_tags = toolbelt::GenerateRandomTags<TAG_WIDTH>(*random, ENVIRONMENT_DISTRACTION_SIGNAL_CNT, env_state_tags, true);
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
    for (size_t i = 0; i < distraction_sig_tags.size(); ++i) {
      std::cout << i << ":";
      distraction_sig_tags[i].Print();
      std::cout << std::endl;
    }

    // Make empty instruction/event libraries. Make hardware.
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();
    eval_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);

    // Configure the mutator
    mutator.SetProgMinFuncCnt(SGP_PROG_MIN_FUNC_CNT);
    mutator.SetProgMaxFuncCnt(SGP_PROG_MAX_FUNC_CNT);
    mutator.SetProgMinFuncLen(SGP_PROG_MIN_FUNC_LEN);
    mutator.SetProgMaxFuncLen(SGP_PROG_MAX_FUNC_LEN);
    mutator.SetProgMaxTotalLen(SGP_PROG_MAX_TOTAL_LEN);
    mutator.SetProgMaxArgVal(SGP_MUT_PROG_MAX_ARG_VAL);
    mutator.SetPerBitTagBitFlipRate(SGP_MUT_PER_BIT__TAG_BFLIP_RATE);
    mutator.SetPerInstSubRate(SGP_MUT_PER_INST__SUB_RATE);
    mutator.SetPerInstInsRate(SGP_MUT_PER_INST__INS_RATE);
    mutator.SetPerInstDelRate(SGP_MUT_PER_INST__DEL_RATE);
    mutator.SetPerFuncSlipRate(SGP_MUT_PER_FUNC__SLIP_RATE);
    mutator.SetPerFuncDupRate(SGP_MUT_PER_FUNC__FUNC_DUP_RATE);
    mutator.SetPerFuncDelRate(SGP_MUT_PER_FUNC__FUNC_DEL_RATE);

    // Configure hardware, etc
    DoConfig__Tasks();
    DoConfig__Hardware();

    switch (RUN_MODE) {
      case RUN_ID__EVO: {
        DoConfig__Experiment();
        DoConfig__Evolution();
        break;
      }
      case RUN_ID__MAPE: {
        DoConfig__Experiment();
        DoConfig__MAPElites();
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

  // === Evolution functions ===
  double GetFitness(agent_t & agent);

  size_t MutateSimilarityThresh(agent_t & agent, emp::Random & rnd);

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
  void InitPopulation__Random();


  // === Systematics Functions ===
  /// Snapshot all programs for current update
  void Snapshot__Programs(size_t u); 
  /// Snapshot population statistics for current update
  void Snapshot__PopulationStats(size_t u);
  /// Snapshot dominant program performance over many trials (only makes sense in context of EA run)
  void Snapshot__Dominant(size_t u); 
  /// Snapshot map from map-elites (only makes sense in context of MAP-Elites run)
  void Snapshot__MAP(size_t u);

  emp::DataFile & AddDominantFile(const std::string & fpath);

  // === Extra SignalGP instruction definitions ===
  // -- Execution control instructions --
  static void Inst_Fork(hardware_t & hw, const inst_t & inst);      
  static void Inst_Terminate(hardware_t & hw, const inst_t & inst); 
  static void Inst_Nand(hardware_t & hw, const inst_t & inst);

  void Inst_Load1(hardware_t & hw, const inst_t & inst);
  void Inst_Load2(hardware_t & hw, const inst_t & inst);
  void Inst_Submit(hardware_t & hw, const inst_t & inst);

  // === SignalGP event definitions ===
  static void HandleEvent__EnvSignal_ED(hardware_t & hw, const event_t & event);
  static void HandleEvent__EnvSignal_IMP(hardware_t & hw, const event_t & event);
  static void DispatchEvent__EnvSignal_ED(hardware_t & hw, const event_t & event);
  static void DispatchEvent__EnvSignal_IMP(hardware_t & hw, const event_t & event);

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

// === SignalGP events ===
// Events.
void Experiment::HandleEvent__EnvSignal_ED(hardware_t & hw, const event_t & event) { hw.SpawnCore(event.affinity, hw.GetMinBindThresh(), event.msg); }
void Experiment::HandleEvent__EnvSignal_IMP(hardware_t & hw, const event_t & event) { return; }
void Experiment::DispatchEvent__EnvSignal_ED(hardware_t & hw, const event_t & event) { hw.QueueEvent(event); }
void Experiment::DispatchEvent__EnvSignal_IMP(hardware_t & hw, const event_t & event) { return; }


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

// === Evolution functions ===
double Experiment::GetFitness(agent_t & agent) {
  const size_t aID = agent.GetID();
  return phen_cache.GetRepresentativePhen(aID).GetScore();
}

size_t Experiment::MutateSimilarityThresh(agent_t & agent, emp::Random & rnd) {
  // TODO: double check functionality of this mutation operator
  if (rnd.P(SGP_MUT_PER_AGENT__SIM_THRESH_RATE)) {
    double new_val = agent.GetSimilarityThreshold() + rnd.GetRandNormal(0, SGP_MUT_PER_AGENT__SIM_THRESH_STD);
    if (new_val > MAX_SIM_THRESH) new_val = MAX_SIM_THRESH;
    else if (new_val < MIN_SIM_THRESH) new_val = MIN_SIM_THRESH;

    agent.SetSimilarityThreshold(new_val);
    return 1;
  }
  return 0;
}

// == utility functions ==

/// Utility function to save environment tags.
void Experiment::SaveEnvTags() {
  // Save out environment states.
  std::ofstream envtags_ofstream(ENVIRONMENT_TAG_FPATH);
  envtags_ofstream << "tag_id,env_tag,tag\n";
  for (size_t i = 0; i < env_state_tags.size(); ++i) {
    envtags_ofstream << i << ",1,"; env_state_tags[i].Print(envtags_ofstream); envtags_ofstream << "\n";
  }
  for (size_t i = 0; i < distraction_sig_tags.size(); ++i) {
    envtags_ofstream << i << ",0,"; distraction_sig_tags[i].Print(envtags_ofstream); envtags_ofstream << "\n";
  }
  envtags_ofstream.close();
}

void Experiment::GenerateEnvTags__FromTagFile() {
  env_state_tags.resize(ENVIRONMENT_STATES, tag_t());
  distraction_sig_tags.resize(ENVIRONMENT_DISTRACTION_SIGNAL_CNT, tag_t());

  std::ifstream tag_fstream(ENVIRONMENT_TAG_FPATH);
  if (!tag_fstream.is_open()) {
    std::cout << "Failed to open " << ENVIRONMENT_TAG_FPATH << ". Exiting..." << std::endl;
    exit(-1);
  }

  std::string cur_line;
  emp::vector<std::string> line_components;

  const size_t tag_id_pos = 0;
  const size_t true_tag_pos = 1;
  const size_t tag_pos = 2;

  std::getline(tag_fstream, cur_line); // Consume header.

  while (!tag_fstream.eof()) {
    std::getline(tag_fstream, cur_line);
    emp::remove_whitespace(cur_line);
    
    if (cur_line == emp::empty_string()) continue;
    emp::slice(cur_line, line_components, ',');

    int tag_id = std::stoi(line_components[tag_id_pos]);
    int true_tag = std::stoi(line_components[true_tag_pos]);

    if (true_tag == 1) {
      // Load environment state tag!
      if (tag_id > env_state_tags.size()) {
        std::cout << "WARNING: tag ID exceeds environment states!" << std::endl;
        continue;
      }
      for (size_t i = 0; i < line_components[tag_pos].size(); ++i) {
        if (i >= TAG_WIDTH) break;
        if (line_components[tag_pos][i] == '1') env_state_tags[tag_id].Set(env_state_tags[tag_id].GetSize() - i - 1, true);
      }
    } else {
      // Load distraction signal tag!
      if (tag_id > distraction_sig_tags.size()) {
        std::cout << "WARNING: tag ID exceeds distraction signals!" << std::endl;
        continue;
      }
      for (size_t i = 0; i < line_components[tag_pos].size(); ++i) {
        if (i >= TAG_WIDTH) break;
        if (line_components[tag_pos][i] == '1') distraction_sig_tags[tag_id].Set(distraction_sig_tags[tag_id].GetSize() - i - 1, true);
      }
    }
  }
  tag_fstream.close();
}

void Experiment::InitPopulation__FromAncestorFile() {
  std::cout << "Initializing population from ancestor file (" << ANCESTOR_FPATH << ")!" << std::endl;
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
  genome_t ancestor_genome(ancestor_prog, SGP_HW_MIN_BIND_THRESH);
  world->Inject(ancestor_genome, POP_SIZE);    // Inject population!
}

void Experiment::InitPopulation__Random() {
  std::cout << "Randomly initializing population!" << std::endl;
  // Inject random agents up to population size.
  for (size_t i = 0; i < POP_SIZE; ++i) {
    program_t ancestor_prog(inst_lib);
    size_t fcnt = random->GetUInt(1, SGP_PROG_MAX_FUNC_CNT);
    for (size_t fID = 0; fID < fcnt; ++fID) {
      hardware_t::Function new_fun;
      new_fun.affinity.Randomize(*random);
      size_t icnt = random->GetUInt(1, emp::Min((size_t)(SGP_PROG_MAX_TOTAL_LEN/SGP_PROG_MAX_FUNC_CNT), SGP_PROG_MAX_FUNC_LEN));
      for (size_t iID = 0; iID < icnt; ++iID) {
        new_fun.PushInst(random->GetUInt(ancestor_prog.GetInstLib()->GetSize()),
                             random->GetInt(SGP_MUT_PROG_MAX_ARG_VAL),
                             random->GetInt(SGP_MUT_PROG_MAX_ARG_VAL),
                             random->GetInt(SGP_MUT_PROG_MAX_ARG_VAL),
                             tag_t());
        new_fun.inst_seq.back().affinity.Randomize(*random);
      }
      ancestor_prog.PushFunction(new_fun);
    }
    genome_t ancestor_genome(ancestor_prog, random->GetDouble(MIN_SIM_THRESH, MAX_SIM_THRESH));
    world->Inject(ancestor_genome, 1);  
  }
  std::cout << "Done randomly initializing population!" << std::endl;
}

// == Systematics functions ==
void Experiment::Snapshot__Programs(size_t u) {
  std::string snapshot_dir = DATA_DIRECTORY + "pop_" + emp::to_string((int)u);
  mkdir(snapshot_dir.c_str(), ACCESSPERMS);
  // For each program in the population, dump the full program description in a single file.
  std::ofstream prog_ofstream(snapshot_dir + "/pop_" + emp::to_string((int)u) + ".pop");
  for (size_t i = 0; i < world->GetSize(); ++i) {
    if (!world->IsOccupied(i)) continue;
    prog_ofstream << "==="<<i<<":"<<world->CalcFitnessID(i)<<","<<world->GetOrg(i).GetSimilarityThreshold()<<"===\n";
    Agent & agent = world->GetOrg(i);
    agent.GetProgram().PrintProgramFull(prog_ofstream);
  }
  prog_ofstream.close();
}

void Experiment::Snapshot__PopulationStats(size_t u) {
  std::string snapshot_dir = DATA_DIRECTORY + "pop_" + emp::to_string((int)update);
  mkdir(snapshot_dir.c_str(), ACCESSPERMS);
  emp::DataFile file(snapshot_dir + "/pop_" + emp::to_string((int)update) + ".csv");
  
  std::function<size_t(void)> get_update = [this](){ return world->GetUpdate(); };
  file.AddFun(get_update, "update", "Update");

  size_t world_id = 0;
  std::function<size_t(void)> get_id = [this, &world_id]() { return world_id; };
  file.AddFun(get_id, "id", "...");

  std::function<size_t(void)> get_func_used = [this, &world_id]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
    return phen.GetFunctionsUsed();
  };
  file.AddFun(get_func_used, "func_used", "...");

  std::function<double(void)> get_inst_ent = [this, &world_id]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
    return phen.GetInstEntropy();
  };
  file.AddFun(get_inst_ent, "inst_entropy", "...");

  std::function<double(void)> get_sim_thresh = [this, &world_id]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
    return phen.GetSimilarityThreshold();
  };
  file.AddFun(get_sim_thresh, "sim_thresh", "...");

  std::function<double(void)> get_score = [this, &world_id]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
    return phen.GetScore();
  };
  file.AddFun(get_score, "score", "...");

  std::function<size_t(void)> get_env_match_score = [this, &world_id]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
    return phen.GetEnvMatchScore();
  };
  file.AddFun(get_env_match_score, "env_matches", "...");

  if (TASKS_ON) {
    std::function<size_t(void)> get_time_all_tasks_credited = [this, &world_id]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
      return phen.GetTimeAllTasksCredited();
    };
    file.AddFun(get_time_all_tasks_credited, "time_all_tasks_credited", "...");

    std::function<size_t(void)> get_unique_tasks_completed = [this, &world_id]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
      return phen.GetUniqueTasksCompleted();
    };
    file.AddFun(get_unique_tasks_completed, "total_unique_tasks_completed", "...");

    std::function<size_t(void)> get_total_wasted_completions = [this, &world_id]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
      return phen.GetTotalWastedCompletions();
    };
    file.AddFun(get_total_wasted_completions, "total_wasted_completions", "...");

    std::function<size_t(void)> get_unique_tasks_credited = [this, &world_id]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
      return phen.GetUniqueTasksCredited();
    };
    file.AddFun(get_unique_tasks_credited, "total_unique_tasks_credited", "...");

    for (size_t i = 0; i < task_set.GetSize(); ++i) {
      std::function<size_t(void)> get_wasted = [this, i, &world_id]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
        return phen.GetWastedCompletions(i);
      };
      file.AddFun(get_wasted, "wasted_"+task_set.GetName(i), "...");

      std::function<size_t(void)> get_completed = [this, i, &world_id]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
        return phen.GetCompleted(i);
      };
      file.AddFun(get_completed, "completed_"+task_set.GetName(i), "...");

      std::function<size_t(void)> get_credited = [this, i, &world_id]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(world_id);
        return phen.GetCredited(i);
      };
      file.AddFun(get_credited, "credited_"+task_set.GetName(i), "...");
    }
  }
  file.PrintHeaderKeys();

  // Loop through population, evaluate, update file.
  for (world_id = 0; world_id < world->GetSize(); ++world_id) {
    if (!world->IsOccupied(world_id)) continue;
    agent_t & agent = world->GetOrg(world_id);
    agent.SetID(world_id);
    this->Evaluate(agent);
    file.Update();
  }
}

void Experiment::Snapshot__Dominant(size_t u) {
  emp_assert(RUN_MODE == RUN_ID__EVO);

  std::string snapshot_dir = DATA_DIRECTORY + "pop_" + emp::to_string((int)u);
  mkdir(snapshot_dir.c_str(), ACCESSPERMS);
  
  emp::vector<double> scores(DOM_SNAPSHOT_TRIAL_CNT,0);
  
  agent_t & dom_agent = world->GetOrg(dom_agent_id);

  begin_agent_eval_sig.Trigger(dom_agent);
  for (size_t i = 0; i < DOM_SNAPSHOT_TRIAL_CNT; ++i) {
    trial_id = 0;
    begin_agent_trial_sig.Trigger(dom_agent);
    do_agent_trial_sig.Trigger(dom_agent);
    end_agent_trial_sig.Trigger(dom_agent);
    // Grab score
    scores[i] = phen_cache.Get(dom_agent.GetID(), trial_id).GetScore();
  }

  // Output stuff to file.
  // Output shit.
  std::ofstream prog_ofstream(snapshot_dir + "/dom_" + emp::to_string((int)u) + ".csv");
  // Fill out the header.
  prog_ofstream << "trial,fitness";
  for (size_t tID = 0; tID < DOM_SNAPSHOT_TRIAL_CNT; ++tID) {
    prog_ofstream << "\n" << tID << "," << scores[tID];
  }
  prog_ofstream.close();
}

void Experiment::Snapshot__MAP(size_t u) {
  emp_assert(RUN_MODE == RUN_ID__MAPE);

  std::string snapshot_dir = DATA_DIRECTORY + "pop_" + emp::to_string((int)u);
  mkdir(snapshot_dir.c_str(), ACCESSPERMS);

  std::ofstream prog_ofstream(snapshot_dir + "/map_" + emp::to_string((int)u) + ".csv");
  // Fill out the header.
  prog_ofstream << "agent_id,trial,fitness,func_used,inst_entropy,sim_thresh";
  
  for (size_t aID = 0; aID < world->GetSize(); ++aID) {
    if (!world->IsOccupied(aID)) continue;
    agent_t & agent = world->GetOrg(aID);


    emp::vector<double> scores(DOM_SNAPSHOT_TRIAL_CNT, 0);
    emp::vector<size_t> func_used(DOM_SNAPSHOT_TRIAL_CNT, 0);

    begin_agent_eval_sig.Trigger(agent);
    for (size_t i = 0; i < DOM_SNAPSHOT_TRIAL_CNT; ++i) {
      trial_id = 0;
      begin_agent_trial_sig.Trigger(agent);
      do_agent_trial_sig.Trigger(agent);
      end_agent_trial_sig.Trigger(agent);
      // Grab score
      scores[i] = phen_cache.Get(agent.GetID(), trial_id).GetScore();
      func_used[i] = phen_cache.Get(agent.GetID(), trial_id).GetFunctionsUsed();
    }
    double entropy = phen_cache.Get(agent.GetID(), 0).GetInstEntropy();
    double sim_thresh = phen_cache.Get(agent.GetID(), 0).GetSimilarityThreshold();

    // Output stuff to file.
    for (size_t tID = 0; tID < DOM_SNAPSHOT_TRIAL_CNT; ++tID) {
      prog_ofstream << "\n" << aID << "," << tID << "," << scores[tID] << "," << func_used[tID] << "," << entropy << "," << sim_thresh;
    }
  }
  prog_ofstream.close();
}

emp::DataFile & Experiment::AddDominantFile(const std::string & fpath="dominant.csv") {
  auto & file = world->SetupFile(fpath);

  std::function<size_t(void)> get_update = [this](){ return world->GetUpdate(); };
  file.AddFun(get_update, "update", "Update");

  std::function<size_t(void)> get_func_used = [this]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
    return phen.GetFunctionsUsed();
  };
  file.AddFun(get_func_used, "func_used", "Number of functions used by program");

  std::function<double(void)> get_inst_ent = [this]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
    return phen.GetInstEntropy();
  };
  file.AddFun(get_inst_ent, "inst_entropy", "Instruction entropy of program");

  std::function<double(void)> get_sim_thresh = [this]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
    return phen.GetSimilarityThreshold();
  };
  file.AddFun(get_sim_thresh, "sim_thresh", "Similarity threshold of program");

  std::function<double(void)> get_score = [this]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
    return phen.GetScore();
  };
  file.AddFun(get_score, "score", "Score of program");

  std::function<size_t(void)> get_env_match_score = [this]() {
    phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
    return phen.GetEnvMatchScore();
  };
  file.AddFun(get_env_match_score, "env_matches", "Number of environment states matched by agent");

  if (TASKS_ON) { 
    std::function<size_t(void)> get_time_all_tasks_credited = [this]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
      return phen.GetTimeAllTasksCredited();
    };
    file.AddFun(get_time_all_tasks_credited, "time_all_tasks_credited", "...");

    std::function<size_t(void)> get_unique_tasks_completed = [this]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
      return phen.GetUniqueTasksCompleted();
    };
    file.AddFun(get_unique_tasks_completed, "total_unique_tasks_completed", "...");

    std::function<size_t(void)> get_total_wasted_completions = [this]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
      return phen.GetTotalWastedCompletions();
    };
    file.AddFun(get_total_wasted_completions, "total_wasted_completions", "...");

    std::function<size_t(void)> get_unique_tasks_credited = [this]() {
      phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
      return phen.GetUniqueTasksCredited();
    };
    file.AddFun(get_unique_tasks_credited, "total_unique_tasks_credited", "...");

    for (size_t i = 0; i < task_set.GetSize(); ++i) {
      std::function<size_t(void)> get_wasted = [this, i]() {
        phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
        return phen.GetWastedCompletions(i);
      };
      file.AddFun(get_wasted, "wasted_"+task_set.GetName(i), "...");

      std::function<size_t(void)> get_completed = [this, i]() {
        phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
        return phen.GetCompleted(i);
      };
      file.AddFun(get_completed, "completed_"+task_set.GetName(i), "...");

      std::function<size_t(void)> get_credited = [this, i]() {
        phenotype_t & phen = phen_cache.GetRepresentativePhen(dom_agent_id);
        return phen.GetCredited(i);
      };
      file.AddFun(get_credited, "credited_"+task_set.GetName(i), "...");
    }
  }
  file.PrintHeaderKeys();
  return file;

}


// == Configuration functions ==
void Experiment::DoConfig__Tasks() {
  // Zero out task inputs.
  for (size_t i = 0; i < MAX_TASK_NUM_INPUTS; ++i) task_inputs[i] = 0;
  // Add tasks to task set.
  // NAND
  task_set.AddTask("NAND", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(~(a&b));
  }, "NAND task");
  // NOT
  task_set.AddTask("NOT", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(~a);
    task.solutions.emplace_back(~b);
  }, "NOT task");
  // ORN
  task_set.AddTask("ORN", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back((a|(~b)));
    task.solutions.emplace_back((b|(~a)));
  }, "ORN task");
  // AND
  task_set.AddTask("AND", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(a&b);
  }, "AND task");
  // OR
  task_set.AddTask("OR", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(a|b);
  }, "OR task");
  // ANDN
  task_set.AddTask("ANDN", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back((a&(~b)));
    task.solutions.emplace_back((b&(~a)));
  }, "ANDN task");
  // NOR
  task_set.AddTask("NOR", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(~(a|b));
  }, "NOR task");
  // XOR
  task_set.AddTask("XOR", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(a^b);
  }, "XOR task");
  // EQU
  task_set.AddTask("EQU", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(~(a^b));
  }, "EQU task");
  // ECHO
  task_set.AddTask("ECHO", [](taskset_t::Task & task, const std::array<task_io_t, MAX_TASK_NUM_INPUTS> & inputs) {
    const task_io_t a = inputs[0], b = inputs[1];
    task.solutions.emplace_back(a);
    task.solutions.emplace_back(b);
  }, "ECHO task");
}

void Experiment::DoConfig__Hardware() {
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
  inst_lib->AddInst("Terminate", Inst_Terminate, 0, "Kill current thread.");

  // Add experiment-specific instructions
  if (TASKS_ON) {
    inst_lib->AddInst("Load-1", [this](hardware_t & hw, const inst_t & inst) { this->Inst_Load1(hw, inst); }, 1, "WM[ARG1] = TaskInput[LOAD_ID]; LOAD_ID++;");
    inst_lib->AddInst("Load-2", [this](hardware_t & hw, const inst_t & inst) { this->Inst_Load2(hw, inst); }, 2, "WM[ARG1] = TASKINPUT[0]; WM[ARG2] = TASKINPUT[1];");
    inst_lib->AddInst("Submit", [this](hardware_t & hw, const inst_t & inst) { this->Inst_Submit(hw, inst); }, 1, "Submit WM[ARG1] as potential task solution.");
    inst_lib->AddInst("Nand", Inst_Nand, 3, "WM[ARG3]=~(WM[ARG1]&WM[ARG2])");
  }

  // Add 1 set state instruction for every possible environment state.
  for (size_t i = 0; i < ENVIRONMENT_STATES; ++i) {
    inst_lib->AddInst("SetState-" + emp::to_string(i),
      [i](hardware_t & hw, const inst_t & inst) {
        hw.SetTrait(TRAIT_ID__STATE, i);
      }, 0, "Set internal state to " + emp::to_string(i));
  }

  // Add events!
  if (SGP_ENVIRONMENT_SIGNALS) {
    // Use event-driven events.
    event_lib->AddEvent("EnvSignal", HandleEvent__EnvSignal_ED, "");
    event_lib->RegisterDispatchFun("EnvSignal", DispatchEvent__EnvSignal_ED);
  } else {
    // Use nop events.
    event_lib->AddEvent("EnvSignal", HandleEvent__EnvSignal_IMP, "");
    event_lib->RegisterDispatchFun("EnvSignal", DispatchEvent__EnvSignal_IMP);
  }

  // Add sensors!
  if (SGP_ACTIVE_SENSORS) {
    // Add sensors to instruction set.
    for (int i = 0; i < ENVIRONMENT_STATES; ++i) {
      inst_lib->AddInst("SenseState-" + emp::to_string(i),
        [this, i](hardware_t & hw, const inst_t & inst) {
          state_t & state = hw.GetCurState();
          state.SetLocal(inst.args[0], this->env_state==i);
        }, 1, "Sense if current environment state is " + emp::to_string(i));
    }
  } else {
    // Add equivalent number of non-functional sensors.
    for (int i = 0; i < ENVIRONMENT_STATES; ++i) {
      inst_lib->AddInst("SenseState-" + emp::to_string(i),
        [this, i](hardware_t & hw, const inst_t & inst) { return; }, 0,
        "Sense if current environment state is " + emp::to_string(i));
    }
  }

  // Configure evaluation hardware.
  eval_hw->SetMinBindThresh(SGP_HW_MIN_BIND_THRESH);
  eval_hw->SetMaxCores(SGP_HW_MAX_CORES);
  eval_hw->SetMaxCallDepth(SGP_HW_MAX_CALL_DEPTH);

  max_inst_entropy = -1 * emp::Log2(1.0/((double)inst_lib->GetSize()));
  std::cout << "Maximum instruction entropy: " << max_inst_entropy << std::endl;

  inst_ent_fun = [](agent_t & agent) {
    emp::vector<inst_t> inst_seq;
    program_t & prog = agent.GetProgram();
    for (size_t i = 0; i < prog.GetSize(); ++i) {
      for (size_t k = 0; k < prog[i].GetSize(); ++k) {
        inst_seq.emplace_back(prog[i][k].id);
      }
    }
    const double ent = emp::ShannonEntropy(inst_seq);
    if (ent < 0.0) return 0.0;
    return ent;
  };
  
  func_cnt_fun = [this](agent_t & agent) {
    return (int)functions_used.size();
  };

  get_sim_thresh_fun = [](agent_t & agent) {
    return agent.GetSimilarityThreshold(); 
  };

}

void Experiment::DoConfig__Evolution() {
  std::cout << "Configure good 'old evolution experiment." << std::endl;

  world->SetPopStruct_Mixed(true);
  world->SetFitFun([this](agent_t & agent) { return this->GetFitness(agent); });

  // Do evaluation!
  do_evaluation_sig.AddAction([this]() {
    best_score = MIN_POSSIBLE_SCORE;
    dom_agent_id = 0;
    for (size_t id = 0; id < world->GetSize(); ++id) {
      // Load and configure the agent.
      agent_t & our_hero = world->GetOrg(id);
      our_hero.SetID(id);
      // Evaluate!
      this->Evaluate(our_hero);
      // Grab the score!
      double score = GetFitness(our_hero);
      if (score > best_score) { best_score = score; dom_agent_id = id; }
    }
    std::cout << "Update: " << update << " Max score: " << best_score << std::endl;
  });

  do_begin_run_setup_sig.AddAction([this]() {
    this->AddDominantFile(DATA_DIRECTORY + "dominant.csv").SetTimingRepeat(SYSTEMATICS_INTERVAL);
  });

  // This assumes that this config function gets called after the general experiment config function.
  do_world_update_sig.AddAction([this]() {
    world->DoMutations(ELITE_SELECT__ELITE_CNT);
  });

  do_pop_snapshot_sig.AddAction([this](size_t u) { this->Snapshot__Dominant(u); });

  // Setup selection
  switch (SELECTION_METHOD) {
    case SELECTION_METHOD_ID__TOURNAMENT: {
      do_selection_sig.AddAction([this]() {
        emp::EliteSelect(*world, ELITE_SELECT__ELITE_CNT, 1);
        emp::TournamentSelect(*world, TOURNAMENT_SIZE, POP_SIZE - ELITE_SELECT__ELITE_CNT);
      });
      break;
    }
    default: {
      std::cout << "Unrecognized selection method id (" << SELECTION_METHOD << "). Exiting..." << std::endl;
      exit(-1);
    }
  }

}

void Experiment::DoConfig__MAPElites() {
  std::cout << "Configure the strange world of MAP-Elites." << std::endl;

  world->SetCache(true);
  world->SetAutoMutate();
  // NOTE: i may need to set mutate on birth to be true!
  world->SetFitFun([this](agent_t & agent) {
    const size_t id = 0;
    agent.SetID(id);
    // Evaluate!
    this->Evaluate(agent);
    // Grab score
    const double score = this->GetFitness(agent);
    if (score > best_score) { best_score = score; dom_agent_id = id; }
    return score;
  });

  emp::vector<size_t> trait_bin_sizes;
  if (MAP_ELITES_AXIS__INST_ENTROPY) {
    std::cout << "Configuring instruction entropy axis" << std::endl;
    world->AddPhenotype("InstEntropy", inst_ent_fun, 0.0, max_inst_entropy + 0.1);
    trait_bin_sizes.emplace_back(MAP_ELITES_AXIS_RES__INST_ENTROPY);
  }
  if (MAP_ELITES_AXIS__FUNCTIONS_USED) {
    std::cout << "Configuring functions used axis" << std::endl;
    world->AddPhenotype("FunctionsUsed", func_cnt_fun, 0, SGP_PROG_MAX_FUNC_CNT+1);
    trait_bin_sizes.emplace_back(SGP_PROG_MAX_FUNC_CNT+1);
  }
  if (MAP_ELITES_AXIS__SIMILARITY_THRESH) {
    std::cout << "Configuring similarity threshold axis" << std::endl;
    world->AddPhenotype("SimilarityThreshold", get_sim_thresh_fun, MIN_SIM_THRESH, MAX_SIM_THRESH+0.01);
    trait_bin_sizes.emplace_back(MAP_ELITES_AXIS_RES__SIMILARITY_THRESH);
  }

  emp::SetMapElites(*world, trait_bin_sizes);

  do_evaluation_sig.AddAction([this]() {
    best_score = MIN_POSSIBLE_SCORE;
  });
  
  do_selection_sig.AddAction([this]() {
    emp::RandomSelect(*world, POP_SIZE);
    std::cout << "Update: " << update << " Best score (from this update): " << best_score << std::endl;
  });

  do_world_update_sig.AddAction([this]() {
    world->ClearCache();
  });

  do_pop_snapshot_sig.AddAction([this](size_t u) { this->Snapshot__MAP(u); });

}
  
void Experiment::DoConfig__Experiment() {
  // Make a data directory. 
  mkdir(DATA_DIRECTORY.c_str(), ACCESSPERMS);
  if (DATA_DIRECTORY.back() != '/') DATA_DIRECTORY += '/';

  world->Reset(); 

  world->SetMutFun([this](agent_t & agent, emp::Random & rnd) {
    return this->mutate_agent(agent, rnd);
  });

  eval_hw->OnBeforeFuncCall([this](hardware_t & hw, size_t fID) {
    functions_used.emplace(fID);
  });
  eval_hw->OnBeforeCoreSpawn([this](hardware_t & hw, size_t fID) {
    functions_used.emplace(fID);
  });

  // Configure score.
  //  - If tasks: 
  //  - else: 
  if (TASKS_ON) {
    calc_score = [this](agent_t & agent) {
      double score = 0;
      phenotype_t & phen = phen_cache.Get(agent.GetID(), trial_id);
      score += phen.GetUniqueTasksCompleted();
      score += phen.GetUniqueTasksCredited();
      if (phen.GetTimeAllTasksCredited()) {
        score += (EVAL_TIME - phen.GetTimeAllTasksCredited());
      }
      score += phen.GetEnvMatchScore();
      return score;
    };
  } else {
    calc_score = [this](agent_t & agent) {
      return phen_cache.Get(agent.GetID(), trial_id).GetEnvMatchScore();
    };
  }

  // Configure mutations
  if (EVOLVE_SIMILARITY_THRESH) {
    mutate_agent = [this](agent_t & agent, emp::Random & rnd) {
      program_t & program = agent.GetProgram();
      size_t mut_cnt = mutator.ApplyMutations(program, rnd);
      mut_cnt += this->MutateSimilarityThresh(agent, rnd);
      return mut_cnt;
    };
  } else {
    mutate_agent = [this](agent_t & agent, emp::Random & rnd) {
      program_t & program = agent.GetProgram();
      return mutator.ApplyMutations(program, rnd);
    };
  }

  // Population initialization!
  switch (POP_INIT_METHOD) {
    case POP_INIT_METHOD_ID__ANCESTOR: {
      do_pop_init_sig.AddAction([this]() {
        this->InitPopulation__FromAncestorFile();
      });
      break;
    }
    case POP_INIT_METHOD_ID__RANDOM: {
      do_pop_init_sig.AddAction([this]() {
        this->InitPopulation__Random();
      });
      break;
    }
    default: {
      std::cout << "Unrecognized population initialization mode (" << POP_INIT_METHOD << "). Exiting..." << std::endl;
      exit(-1);
    }
  }

  // Configure signals
  // - Pop snapshots!
  do_pop_snapshot_sig.AddAction([this](size_t u) { this->Snapshot__Programs(u); });
  do_pop_snapshot_sig.AddAction([this](size_t u) { this->Snapshot__PopulationStats(u); });

  // - Do world update
  do_world_update_sig.AddAction([this]() {
    if (update % POP_SNAPSHOT_INTERVAL == 0) do_pop_snapshot_sig.Trigger(update);
    world->Update(); 
  });

  // - Do begin run setup (common to MAP-elites and regular EA)
  do_begin_run_setup_sig.AddAction([this]() {
    std::cout << "Doing initial run setup." << std::endl;
    // Setup phenotype task counts to match actual task counts.
    for (size_t aID = 0; aID < POP_SIZE; ++aID) {
      for (size_t tID = 0; tID < TRIAL_CNT; ++tID) {
        phen_cache.Get(aID, tID).SetTaskCnt(task_set.GetSize());
      }
    }
    // Setup systematics/fitness tracking.
    // TODO: ask Emily about issue with setting up systematics file
    // auto & sys_file = world->SetupSystematicsFile("default_systematics", DATA_DIRECTORY + "systematics.csv");
    // sys_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
    auto & fit_file = world->SetupFitnessFile(DATA_DIRECTORY + "fitness.csv");
    fit_file.SetTimingRepeat(FITNESS_INTERVAL);
    do_pop_init_sig.Trigger();
  });

  // - Begin agent eval signal
  begin_agent_eval_sig.AddAction([this](agent_t & agent) {
    eval_hw->SetProgram(agent.GetProgram());
  });

  if (EVOLVE_SIMILARITY_THRESH) {
    // Set similarity threshold on eval hardware at beginning of evaluation.
    begin_agent_eval_sig.AddAction([this](agent_t & agent) {
      eval_hw->SetMinBindThresh(agent.GetSimilarityThreshold());
    });
  }

  end_agent_eval_sig.AddAction([this](agent_t & agent) {
    phen_cache.SetRepresentativeEval(agent.GetID());
  });

  // - Begin trial info!
  begin_agent_trial_sig.AddAction([this](agent_t & agent) {
    // 1) reset environment state
    env_state = (size_t)-1;
    // 2) Reset tasks. 
    this->ResetTasks();
    input_load_id = 0;
    // 3) Reset hardware.
    functions_used.clear();
    eval_hw->ResetHardware();
    eval_hw->SetTrait(TRAIT_ID__STATE, -1);
    // 4) Reset phenotype
    phen_cache.Get(agent.GetID(), trial_id).Reset();
    // For now, not spawning a core... 
  });

  do_agent_trial_sig.AddAction([this](agent_t & agent) {
    for (trial_time = 0; trial_time < EVAL_TIME; ++trial_time) {
      // 1) Advance environment.
      do_env_advance_sig.Trigger();
      // 2) Advance agent.
      do_agent_advance_sig.Trigger(agent);
    }
  });
  
  end_agent_trial_sig.AddAction([this](agent_t & agent) {
    const size_t agent_id = agent.GetID();
    phenotype_t & phen = phen_cache.Get(agent_id, trial_id);
    // Record everything that must be recorded post-trial
    phen.SetFunctionsUsed(this->func_cnt_fun(agent));
    phen.SetInstEntropy(this->inst_ent_fun(agent));
    phen.SetSimilarityThreshold(agent.GetSimilarityThreshold());
    phen.SetTimeAllTasksCredited(task_set.GetAllTasksCreditedTime());
    phen.SetUniqueTasksCompleted(task_set.GetUniqueTasksCompleted());
    phen.SetUniqueTasksCredited(task_set.GetUniqueTasksCredited());
    phen.SetTotalWastedCompletions(task_set.GetTotalTasksWasted());
    for (size_t taskID = 0; taskID < task_set.GetSize(); ++taskID) {
      phen.SetCredited(taskID, task_set.GetTask(taskID).GetCreditedCnt());
      phen.SetCompleted(taskID, task_set.GetTask(taskID).GetCompletionCnt());
      phen.SetWastedCompletions(taskID, task_set.GetTask(taskID).GetWastedCompletionsCnt());
    }
    phen.SetScore(calc_score(agent));
  });

  do_agent_advance_sig.AddAction([this](agent_t & agent) {
    const size_t agent_id = agent.GetID();
    eval_hw->SingleProcess();
    if ((size_t)eval_hw->GetTrait(TRAIT_ID__STATE) == env_state) {
      phen_cache.Get(agent_id, trial_id).IncEnvMatchScore();
    }
  });

  switch(ENVIRONMENT_CHANGE_METHOD) {
    case ENV_CHG_METHOD_ID__RANDOM: {
      do_env_advance_sig.AddAction([this]() {
        if (env_state == (size_t)-1 || random->P(ENVIRONMENT_CHANGE_PROB)) {
          // Trigger change!
          // 1) Change the environment to a random state.
          env_state = random->GetUInt(ENVIRONMENT_STATES);
          // 2) Trigger environment state event.
          eval_hw->TriggerEvent("EnvSignal", env_state_tags[env_state]);
        }
      });
      break;
    }
    case ENV_CHG_METHOD_ID__REGULAR: {
      do_env_advance_sig.AddAction([this]() {
        if (env_state == (size_t)-1 || ((trial_time % ENVIRONMENT_CHANGE_INTERVAL) == 0)) {
          // Trigger change!
          // 1) Change the environment to a random state.
          env_state = random->GetUInt(ENVIRONMENT_STATES);
          // 2) Trigger environment state event.
          eval_hw->TriggerEvent("EnvSignal", env_state_tags[env_state]);
        }
      });
      break;
    }
    default: {
      std::cout << "Unrecognized environment change method. Exiting..." << std::endl;
      exit(-1);
    }
  }

  // If distraction signals...
  if (ENVIRONMENT_DISTRACTION_SIGNALS) {
    do_env_advance_sig.AddAction([this]() {
      if (random->P(ENVIRONMENT_DISTRACTION_SIGNAL_PROB)) {
        const size_t id = random->GetUInt(distraction_sig_tags.size());
        eval_hw->TriggerEvent("EnvSignal", distraction_sig_tags[id]);
      }
    });
  }
}

void Experiment::DoConfig__Analysis() {

}

#endif
