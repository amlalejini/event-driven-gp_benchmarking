#ifndef AB_RESP_EXPERIMENT_H
#define AB_RESP_EXPERIMENT_H

// includes
#include <iostream>
#include <string>
#include <utility>
#include <fstream>
#include <sys/stat.h>
#include <algorithm>
#include <functional>
#include <deque>
#include <unordered_set>
#include <unordered_map>

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

#include "../../utility_belt/source/utilities.h"

#include "ab_resp-config.h"

// Globals
constexpr size_t RUN_ID__EXP = 0;
constexpr size_t RUN_ID__ANALYSIS = 1;

constexpr size_t SIGNAL_TAG_GEN_METHOD_ID__RAND = 0;
constexpr size_t SIGNAL_TAG_GEN_METHOD_ID__LOAD = 1;

constexpr size_t TAG_WIDTH = 16;

/// Class to manage AB response experiment.
class Experiment {
public:
  // A few forward declarations.
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
  // - Agent/phenotype aliases
  using agent_t = Agent;
  using phenotype_t = Phenotype;
  using phen_cache_t = PhenotypeCache;
  // - World aliases
  using world_t = emp::World<agent_t>;

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

  struct Phenotype {

  };

  /// Cache for storing evaluated phenotypes. 
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

      phenotype_t & GetRepresentative(size_t agent_id) {
        return Get(agent_id, agent_representative_eval[agent_id]);
      }
      
      // TODO: better way of setting representative evaluation
      // void SetRepresentativeEval(size_t agent_id) {
      //   emp_assert(agent_id < agent_cnt);
      //   // agent_representative_eval[agent_id] = eval_id;
      //   double score = Get(agent_id, 0).GetScore();
      //   size_t repID = 0;
      //   // Return the minimum score!
      //   for (size_t eID = 1; eID < eval_cnt; ++eID) {
      //     phenotype_t & phen = Get(agent_id, eID);
      //     if (phen.GetScore() < score) { score = phen.GetScore(); repID = eID; }
      //   }
      //   agent_representative_eval[agent_id] = repID;
      // }
  };

protected:
  // Configuration parameters.
  // -- DEFAULT_GROUP --
  size_t RUN_MODE;
  int RANDOM_SEED;
  size_t POP_SIZE;
  size_t GENERATIONS;
  std::string ANCESTOR_FPATH;
  // -- EVALUATION_GROUP --
  size_t EVALUATION_CNT;
  size_t SIGNAL_CNT;
  size_t RESPONSE_CNT;
  size_t SIGNAL_TAG_GENERATION_METHOD;
  std::string SIGNAL_TAG_FPATH;
  // -- SELECTION_GROUP --
  size_t TOURNAMENT_SIZE;
  size_t SELECTION_METHOD;
  size_t ELITE_SELECT__ELITE_CNT;
  // -- SGP_FUNCTION_REGULATION_GROUP --
  size_t SIMILARITY_ADJUSTMENT_METHOD;
  size_t REF_MOD_ADJUSTMENT_TYPE;
  double REF_MOD_ADJUSTMENT_VALUE;
  bool MODIFY_REG;
  // -- SGP_PROGRAM_GROUP --
  size_t SGP_PROG_MAX_FUNC_CNT;
  size_t SGP_PROG_MIN_FUNC_CNT;
  size_t SGP_PROG_MAX_FUNC_LEN;
  size_t SGP_PROG_MIN_FUNC_LEN;
  size_t SGP_PROG_MAX_TOTAL_LEN;
  // -- SGP_HARDWARE_GROUP --
  size_t SGP_HW_MAX_CORES;
  size_t SGP_HW_MAX_CALL_DEPTH;
  double SGP_HW_MIN_BIND_THRESH;
  // -- SGP_MUTATION_GROUP --
  int SGP__PROG_MAX_ARG_VAL;
  double SGP__PER_BIT__TAG_BFLIP_RATE;
  double SGP__PER_INST__SUB_RATE;
  double SGP__PER_INST__INS_RATE;
  double SGP__PER_INST__DEL_RATE;
  double SGP__PER_FUNC__SLIP_RATE;
  double SGP__PER_FUNC__FUNC_DUP_RATE;
  double SGP__PER_FUNC__FUNC_DEL_RATE;
  // -- DATA_GROUP --
  size_t SYSTEMATICS_INTERVAL;
  size_t POP_SNAPSHOT_INTERVAL;
  std::string DATA_DIRECTORY;

  // Experiment member variables. 
  emp::Ptr<emp::Random> random;     ///< Random number generator
  emp::Ptr<world_t> world;          ///< Empirical world for evolution

  emp::Ptr<inst_lib_t> inst_lib;    ///< SignalGP instruction library
  emp::Ptr<event_lib_t> event_lib;  ///< SignalGP event library

  emp::Ptr<hardware_t> eval_hw;     ///< SignalGP virtual hardware used for evaluation

  toolbelt::SignalGPMutator<hardware_t> mutator;

  size_t update;    ///< Current update (generation) of experiment
  size_t eval_id;  
  size_t trial_id; 

  emp::vector<size_t> switch_trial_by_eval;

  size_t response_time; ///< Current time during an agent's response to a signal.

  size_t dom_agent_id;

  phen_cache_t phen_cache;

  emp::vector<tag_t> signal_tags;
  emp::vector<size_t> signal_responses;
  emp::vector<tag_t> feedback_tags;

  
  // Run signals
  emp::Signal<void(void)> do_begin_run_setup_sig;   ///< Triggered at begining of run.
  emp::Signal<void(void)> do_pop_init_sig;          ///< Triggered during run setup. Defines way population is initialized.
  
  emp::Signal<void(void)> do_evaluation_sig;        ///< Triggered during run step. Should trigger population-wide agent evaluation.
  emp::Signal<void(void)> do_selection_sig;         ///< Triggered during run step. Should trigger selection (which includes selection, reproduction, and mutation).
  emp::Signal<void(void)> do_world_update_sig;      ///< Triggered during run step. Should trigger world->Update(), and whatever else should happen right before/after population turnover.
  emp::Signal<void(void)> do_analysis_sig;          ///< Triggered for analysis mode. 

  // Systematics signals
  emp::Signal<void(size_t)> do_pop_snapshot_sig;

  // Evaluation Signals
  emp::Signal<void(agent_t &)> begin_agent_eval_sig;  ///< Triggered at beginning of agent evaluation (might be multiple trials)
  emp::Signal<void(agent_t &)> end_agent_eval_sig;  ///< Triggered at beginning of agent evaluation (might be multiple trials)

  emp::Signal<void(agent_t &)> begin_agent_maze_trial_sig; ///< Triggered at the beginning of an agent trial.
  emp::Signal<void(agent_t &)> do_agent_maze_trial_sig; ///< Triggered at the beginning of an agent trial.
  emp::Signal<void(agent_t &)> end_agent_maze_trial_sig; ///< Triggered at the beginning of an agent trial.

  emp::Signal<void(agent_t &)> do_agent_advance_sig;
  emp::Signal<void(agent_t &)> after_agent_action_sig;

  void Evaluate(agent_t & agent) {
    // TODO: write evaluate function
  }

  /// Scratch/test function. 
  /// I use this function to test experiment functionality. (this is where I shove debug code)
  /// Not actually called/used during an experiment.
  void Test() {
    
  }

public:
  Experiment(const ABRespConfig & config) 
    : mutator(), update(0), eval_id(0), trial_id(0),
      switch_trial_by_eval(0), response_time(0),
      dom_agent_id(0), phen_cache(0,0),
      signal_tags(0), signal_responses(0), feedback_tags(0)
  {
    // Load configuration parameters!
    // -- Default Group --
    RUN_MODE = config.RUN_MODE();
    RANDOM_SEED = config.RANDOM_SEED();
    POP_SIZE = config.POP_SIZE();
    GENERATIONS = config.GENERATIONS();
    ANCESTOR_FPATH = config.ANCESTOR_FPATH();
    // -- Evaluation Group --
    EVALUATION_CNT = config.EVALUATION_CNT();
    SIGNAL_CNT = config.SIGNAL_CNT();
    RESPONSE_CNT = config.RESPONSE_CNT();
    SIGNAL_TAG_GENERATION_METHOD = config.SIGNAL_TAG_GENERATION_METHOD();
    SIGNAL_TAG_FPATH = config.SIGNAL_TAG_FPATH();
    // -- Selection Group --
    TOURNAMENT_SIZE = config.TOURNAMENT_SIZE();
    SELECTION_METHOD = config.SELECTION_METHOD();
    ELITE_SELECT__ELITE_CNT = config.ELITE_SELECT__ELITE_CNT();
    // -- Function Regulation Group --
    SIMILARITY_ADJUSTMENT_METHOD = config.SIMILARITY_ADJUSTMENT_METHOD();
    REF_MOD_ADJUSTMENT_TYPE = config.REF_MOD_ADJUSTMENT_TYPE();
    REF_MOD_ADJUSTMENT_VALUE = config.REF_MOD_ADJUSTMENT_VALUE();
    MODIFY_REG = config.MODIFY_REG();
    // -- Program Group --
    SGP_PROG_MAX_FUNC_CNT = config.SGP_PROG_MAX_FUNC_CNT();
    SGP_PROG_MIN_FUNC_CNT = config.SGP_PROG_MIN_FUNC_CNT();
    SGP_PROG_MAX_FUNC_LEN = config.SGP_PROG_MAX_FUNC_LEN();
    SGP_PROG_MIN_FUNC_LEN = config.SGP_PROG_MIN_FUNC_LEN();
    SGP_PROG_MAX_TOTAL_LEN = config.SGP_PROG_MAX_TOTAL_LEN();
    // -- Hardware Group --
    SGP_HW_MAX_CORES = config.SGP_HW_MAX_CORES();
    SGP_HW_MAX_CALL_DEPTH = config.SGP_HW_MAX_CALL_DEPTH();
    SGP_HW_MIN_BIND_THRESH = config.SGP_HW_MIN_BIND_THRESH();
    // -- Mutation Group -- 
    SGP__PROG_MAX_ARG_VAL = config.SGP__PROG_MAX_ARG_VAL();
    SGP__PER_BIT__TAG_BFLIP_RATE = config.SGP__PER_BIT__TAG_BFLIP_RATE();
    SGP__PER_INST__SUB_RATE = config.SGP__PER_INST__SUB_RATE();
    SGP__PER_INST__INS_RATE = config.SGP__PER_INST__INS_RATE();
    SGP__PER_INST__DEL_RATE = config.SGP__PER_INST__DEL_RATE();
    SGP__PER_FUNC__SLIP_RATE = config.SGP__PER_FUNC__SLIP_RATE();
    SGP__PER_FUNC__FUNC_DUP_RATE = config.SGP__PER_FUNC__FUNC_DUP_RATE();
    SGP__PER_FUNC__FUNC_DEL_RATE = config.SGP__PER_FUNC__FUNC_DEL_RATE();
    // -- Data Group --
    SYSTEMATICS_INTERVAL = config.SYSTEMATICS_INTERVAL();
    POP_SNAPSHOT_INTERVAL = config.POP_SNAPSHOT_INTERVAL();
    DATA_DIRECTORY = config.DATA_DIRECTORY();

    // Create a new random number generator
    random = emp::NewPtr<emp::Random>(RANDOM_SEED);

    // Make the world!
    world = emp::NewPtr<world_t> (*random, "World");

    // Configure the phenotype cache
    phen_cache.Resize(POP_SIZE, EVALUATION_CNT);

    switch_trial_by_eval.resize(EVALUATION_CNT, 0);

    if (EVALUATION_CNT < 1) {
      std::cout << "Cannot run experiment with EVALUATION_CNT < 1. Exiting." << std::endl;
      exit(-1);
    }

    // Configure signals/responses.
    switch (SIGNAL_TAG_GENERATION_METHOD) {
      case SIGNAL_TAG_GEN_METHOD_ID__RAND: {
        emp::vector<tag_t> tags = toolbelt::GenerateRandomTags<TAG_WIDTH>(*random, 2*SIGNAL_CNT, true);
        for (size_t i = 0; i < SIGNAL_CNT; ++i) {
          signal_tags.emplace_back(tags[i]);
          feedback_tags.emplace_back(tags[SIGNAL_CNT + i]);
        }
        break;
      }
      case SIGNAL_TAG_GEN_METHOD_ID__LOAD: {
        // TODO: load tags from file
        break;
      }
      default: {
        std::cout << "Unrecognized SIGNAL_TAG_GENERATION_METHOD (" << SIGNAL_TAG_GENERATION_METHOD << "). Exiting..." << std::endl;
        exit(-1);
      }
    }

    // Print Signal/feedback tags
    std::cout << "Signal/Feedback tags:" << std::endl;
    for (size_t i = 0; i < SIGNAL_CNT; ++i) {
      std::cout << i << ": "; signal_tags[i].Print();
      std::cout << "==>"; feedback_tags[i].Print();
      std::cout << std::endl; 
    }

    // Make empty instruction/event libraries
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();
    eval_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);

    // Configure the mutator
    mutator.SetProgMinFuncCnt(SGP_PROG_MIN_FUNC_CNT);
    mutator.SetProgMaxFuncCnt(SGP_PROG_MAX_FUNC_CNT);
    mutator.SetProgMinFuncLen(SGP_PROG_MIN_FUNC_LEN);
    mutator.SetProgMaxFuncLen(SGP_PROG_MAX_FUNC_LEN);
    mutator.SetProgMaxTotalLen(SGP_PROG_MAX_TOTAL_LEN);
    mutator.SetProgMaxArgVal(SGP__PROG_MAX_ARG_VAL);
    mutator.SetPerBitTagBitFlipRate(SGP__PER_BIT__TAG_BFLIP_RATE);
    mutator.SetPerInstSubRate(SGP__PER_INST__SUB_RATE);
    mutator.SetPerInstInsRate(SGP__PER_INST__INS_RATE);
    mutator.SetPerInstDelRate(SGP__PER_INST__DEL_RATE);
    mutator.SetPerFuncSlipRate(SGP__PER_FUNC__SLIP_RATE);
    mutator.SetPerFuncDupRate(SGP__PER_FUNC__FUNC_DUP_RATE);
    mutator.SetPerFuncDelRate(SGP__PER_FUNC__FUNC_DEL_RATE);

    // DoConfig__Hardware();

    switch (RUN_MODE) {
      case RUN_ID__EXP: {
        // DoConfig__Experiment();
        break;
      }
      case RUN_ID__ANALYSIS: {
        // DoConfig__Analysis();
        break;
      }
      default: {
        std::cout << "Unrecognized RUN_MODE (" << RUN_MODE << "). Exiting..." << std::endl;
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

  // TODO: function documentation!

  // === Do experiment functions! ===
  void Run();
  void RunStep();

  // === Do config functions! ===
  void DoConfig__Hardware();
  void DoConfig__Experiment();
  void DoConfig__Analysis();

  // === Evolution functions ===
  size_t Mutate(agent_t & agent, emp::Random & rnd);
  double CalcFitness(agent_t & agent);

  // === Misc. utility functions ===
  void InitPopulation__FromAncestorFile();

  void GenerateSignalTags__FromTagFile();
  void SaveSignalTags();

  // === Systematics functions ===
  void Snapshot__Programs(size_t u);

  emp::DataFile & AddDominantFile(const std::string & fpath="dominant.csv");

  // === Extra SGP instruction definitions ===
  // -- Execution control instructions (that make use of function regulation) --
  static void Inst_Call(hardware_t & hw, const inst_t & inst);      
  static void Inst_Fork(hardware_t & hw, const inst_t & inst);      
  static void Inst_Terminate(hardware_t & hw, const inst_t & inst); 
  // -- Sensor instructions --

  // === SGP event handlers/dispatchers ===
  void EventDispatch__Signal(hardware_t & hw, const event_t & event);
  void EventHandler__Signal(hardware_t & hw, const event_t & event);
  
};

// ================== Instruction definition implementations ==================
void Experiment::Inst_Call(hardware_t & hw, const inst_t & inst) {
  hw.CallFunction(inst.affinity, hw.GetMinBindThresh(), true);
}

void Experiment::Inst_Fork(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  hw.SpawnCore(inst.affinity, hw.GetMinBindThresh(), state.local_mem, false, true);
}

void Experiment::Inst_Terminate(hardware_t & hw, const inst_t & inst)  {
  // Pop all the call states from current core.
  exec_stk_t & core = hw.GetCurCore();
  core.resize(0);
}

// ================== Run experiment implementations ==================
void Experiment::Run() {
  switch (RUN_MODE) {
    case RUN_ID__EXP: {
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

// ================== Misc. utility function implementations ==================
void Experiment::GenerateSignalTags__FromTagFile() {
  signal_tags.resize(SIGNAL_CNT, tag_t());
  feedback_tags.resize(SIGNAL_CNT, tag_t());

  std::ifstream tag_fstream(SIGNAL_TAG_FPATH);

  if (!tag_fstream.is_open()) {
    std::cout << "Failed to open SIGNAL_TAG_FPATH (" << SIGNAL_TAG_FPATH << "). Exiting..." << std::endl;
    exit(-1);
  }

  std::string cur_line;
  emp::vector<std::string> line_components;
  std::getline(tag_fstream, cur_line); // Consume header.
  
  // Expectation: 0: SignalID, Signal Tag, Response Feedback Signal Tag

  while (!tag_fstream.eof()) {
    std::getline(tag_fstream, cur_line);
    emp::remove_whitespace(cur_line);
    if (cur_line == emp::empty_string()) continue;
    emp::slice(cur_line, line_components, ',');
    int tag_id = std::stoi(line_components[0]);

    if (tag_id > SIGNAL_CNT) {
      std::cout << "WARNING: signal ID exceeds SIGNAL_CNT (" << SIGNAL_CNT << "). Ignoring..." << std::endl;
      continue;
    }

    for (size_t i = 0; i < line_components[1].size(); ++i) {
      if (i >= TAG_WIDTH) break;
      if (line_components[1][i] == '1') signal_tags[tag_id].Set(signal_tags[tag_id].GetSize() - i - 1, true);
    }

    for (size_t i = 0; i < line_components[2].size(); ++i) {
      if (i >= TAG_WIDTH) break;
      if (line_components[1][i] == '1') feedback_tags[tag_id].Set(feedback_tags[tag_id].GetSize() - i - 1, true);
    }

  }
  tag_fstream.close();
}

void Experiment::SaveSignalTags() {
  // Save out the signal tags.
  std::ofstream signaltags_ofstream(SIGNAL_TAG_FPATH);
  signaltags_ofstream << "signal_id,signal_tag,feedback_tag\n";
  for (size_t i = 0; i < SIGNAL_CNT; ++i) {
    signaltags_ofstream << i << ","; signal_tags[i].Print(signaltags_ofstream); 
    signaltags_ofstream << ","; feedback_tags[i].Print(signaltags_ofstream);
    signaltags_ofstream << "\n";
  }
  signaltags_ofstream.close();
}

// ================== Evolution function implementations ==================
size_t Experiment::Mutate(agent_t & agent) {
  program_t & program = agent.GetGenome();
  return mutator.ApplyMutations(program, *random);
}

// ================== Configuration implementations ==================


#endif