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

constexpr size_t REF_MOD_ADJUSTMENT_TYPE_ID__ADD = 0;
constexpr size_t REF_MOD_ADJUSTMENT_TYPE_ID__MULT = 1;

constexpr size_t SIMILARITY_ADJUSTMENT_METHOD_ID__ADD = 0;
constexpr size_t SIMILARITY_ADJUSTMENT_METHOD_ID__MULT = 1;

constexpr size_t SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD_ID__RAND = 0;
constexpr size_t SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD_ID__HALF = 1;

constexpr size_t SELECTION_METHOD_ID__TOURNAMENT = 0;

constexpr size_t TAG_WIDTH = 16;

constexpr double MIN_POSSIBLE_SCORE = -32767;

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
  size_t TRIAL_CNT;
  size_t SIGNAL_CNT;
  size_t RESPONSE_CNT;
  size_t SIGNAL_TAG_GENERATION_METHOD;
  std::string SIGNAL_TAG_FPATH;
  size_t SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD;
  size_t SIGNAL_RESPONSE_MAPPING_CHANGE_MIN_TRIAL;
  size_t SIGNAL_RESPONSE_MAPPING_CHANGE_MAX_TRIAL;
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

  emp::Signal<void(agent_t &)> begin_agent_trial_sig; ///< Triggered at the beginning of an agent trial.
  emp::Signal<void(agent_t &)> do_agent_trial_sig; ///< Triggered at the beginning of an agent trial.
  emp::Signal<void(agent_t &)> end_agent_trial_sig; ///< Triggered at the beginning of an agent trial.

  emp::Signal<void(agent_t &)> do_agent_advance_sig;
  emp::Signal<void(agent_t &)> after_agent_action_sig;
  emp::Signal<void(void)> reset_signal_mapping_change_trial_sig;

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
    TRIAL_CNT = config.TRIAL_CNT();
    SIGNAL_CNT = config.SIGNAL_CNT();
    RESPONSE_CNT = config.RESPONSE_CNT();
    SIGNAL_TAG_GENERATION_METHOD = config.SIGNAL_TAG_GENERATION_METHOD();
    SIGNAL_TAG_FPATH = config.SIGNAL_TAG_FPATH();
    SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD = config.SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD();
    SIGNAL_RESPONSE_MAPPING_CHANGE_MIN_TRIAL = config.SIGNAL_RESPONSE_MAPPING_CHANGE_MIN_TRIAL();
    SIGNAL_RESPONSE_MAPPING_CHANGE_MAX_TRIAL = config.SIGNAL_RESPONSE_MAPPING_CHANGE_MAX_TRIAL();
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
  world->Inject(ancestor_prog, POP_SIZE);    // Inject population!
}

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

// ================== Systematics functions ==================
// TODO: move this over to utilities belt!
void Experiment::Snapshot__Programs(size_t u) {
  std::string snapshot_dir = DATA_DIRECTORY + "pop_" + emp::to_string((int)u);
  mkdir(snapshot_dir.c_str(), ACCESSPERMS);
  // For each program in the population, dump the full program description in a single file.
  std::ofstream prog_ofstream(snapshot_dir + "/pop_" + emp::to_string((int)u) + ".pop");
  for (size_t i = 0; i < world->GetSize(); ++i) {
    if (!world->IsOccupied(i)) continue;
    prog_ofstream << "==="<<i<<":"<<world->CalcFitnessID(i)<<"===\n";
    Agent & agent = world->GetOrg(i);
    agent.program.PrintProgramFull(prog_ofstream);
  }
  prog_ofstream.close();
}

// ================== Evolution function implementations ==================
size_t Experiment::Mutate(agent_t & agent, emp::Random & rnd) {
  program_t & program = agent.GetGenome();
  return mutator.ApplyMutations(program, rnd);
}

double Experiment::CalcFitness(agent_t & agent) {
  // TODO: implement fitness function
  return 0.0;
}

// ================== Configuration implementations ==================
void Experiment::DoConfig__Hardware() {
  // Setup the instruction set
  // - Standard instructions
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
  inst_lib->AddInst("Return", hardware_t::Inst_Return, 0, "Return from current function if possible.");
  inst_lib->AddInst("SetMem", hardware_t::Inst_SetMem, 2, "Local memory: Arg1 = numerical value of Arg2");
  inst_lib->AddInst("CopyMem", hardware_t::Inst_CopyMem, 2, "Local memory: Arg1 = Arg2");
  inst_lib->AddInst("SwapMem", hardware_t::Inst_SwapMem, 2, "Local memory: Swap values of Arg1 and Arg2.");
  inst_lib->AddInst("Input", hardware_t::Inst_Input, 2, "Input memory Arg1 => Local memory Arg2.");
  inst_lib->AddInst("Output", hardware_t::Inst_Output, 2, "Local memory Arg1 => Output memory Arg2.");
  inst_lib->AddInst("Commit", hardware_t::Inst_Commit, 2, "Local memory Arg1 => Shared memory Arg2.");
  inst_lib->AddInst("Pull", hardware_t::Inst_Pull, 2, "Shared memory Arg1 => Shared memory Arg2.");
  inst_lib->AddInst("Nop", hardware_t::Inst_Nop, 0, "No operation.");

  inst_lib->AddInst("Call", Inst_Call, 0, "Call function that best matches call affinity.", emp::ScopeType::BASIC, 0, {"affinity"});
  inst_lib->AddInst("Fork", Inst_Fork, 0, "Fork a new thread. Local memory contents of callee are loaded into forked thread's input memory.", emp::ScopeType::BASIC, 0, {"affinity"});
  inst_lib->AddInst("Terminate", Inst_Terminate, 0, "Kill current thread.");

  // TODO: any experiment-specific instructions

  // Regulatory instructions
  switch(REF_MOD_ADJUSTMENT_TYPE) {
    case REF_MOD_ADJUSTMENT_TYPE_ID__ADD: {
      // When applying regulation to a function's reference modifier, do so by adding/subtracting ref mod adjustment value.

      inst_lib->AddInst("Promote", [this](hardware_t & hw, const inst_t & inst) {
        emp::vector<size_t> targets(hw.FindBestFuncMatch(inst.affinity, 0.0, MODIFY_REG));
        size_t targetID;
        if (targets.empty()) return; 
        if (targets.size() == 1) targetID = targets[0];
        else targetID = targets[random->GetUInt(targets.size())];
        program_t & program = hw.GetProgram();
        double cur_mod = program[targetID].GetRefModifier();
        program[targetID].SetRefModifier(cur_mod + REF_MOD_ADJUSTMENT_VALUE);
      }, 0, "Up regulate target function. Use tag to determine function target.", emp::ScopeType::BASIC, 0, {"affinity"});

      inst_lib->AddInst("Repress", [this](hardware_t & hw, const inst_t & inst) {
        emp::vector<size_t> targets(hw.FindBestFuncMatch(inst.affinity, 0.0, MODIFY_REG));
        size_t targetID;
        if (targets.empty()) return; 
        if (targets.size() == 1) targetID = targets[0];
        else targetID = targets[random->GetUInt(targets.size())];
        program_t & program = hw.GetProgram();
        double cur_mod = program[targetID].GetRefModifier();
        program[targetID].SetRefModifier(cur_mod - REF_MOD_ADJUSTMENT_VALUE);
      }, 0, "Down regulate target function. Use tag to determine function target.", emp::ScopeType::BASIC, 0, {"affinity"});

      break;
    }
    case REF_MOD_ADJUSTMENT_TYPE_ID__MULT: {

      emp_assert(REF_MOD_ADJUSTMENT_VALUE != 0);

      inst_lib->AddInst("Promote", [this](hardware_t & hw, const inst_t & inst) {
        emp::vector<size_t> targets(hw.FindBestFuncMatch(inst.affinity, 0.0, MODIFY_REG));
        size_t targetID;
        if (targets.empty()) return; 
        if (targets.size() == 1) targetID = targets[0];
        else targetID = targets[random->GetUInt(targets.size())];
        program_t & program = hw.GetProgram();
        double cur_mod = program[targetID].GetRefModifier();
        program[targetID].SetRefModifier(cur_mod * REF_MOD_ADJUSTMENT_VALUE);
      }, 0, "Up regulate target function. Use tag to determine function target.", emp::ScopeType::BASIC, 0, {"affinity"});

      inst_lib->AddInst("Repress", [this](hardware_t & hw, const inst_t & inst) {
        emp::vector<size_t> targets(hw.FindBestFuncMatch(inst.affinity, 0.0, MODIFY_REG));
        size_t targetID;
        if (targets.empty()) return; 
        if (targets.size() == 1) targetID = targets[0];
        else targetID = targets[random->GetUInt(targets.size())];
        program_t & program = hw.GetProgram();
        double cur_mod = program[targetID].GetRefModifier();
        program[targetID].SetRefModifier(cur_mod * (1/REF_MOD_ADJUSTMENT_VALUE));
      }, 0, "Down regulate target function. Use tag to determine function target.", emp::ScopeType::BASIC, 0, {"affinity"});
      
      break;
    }
    default: {
      std::cout << "Unrecognized REF_MOD_ADJUSTMENT_TYPE (" << REF_MOD_ADJUSTMENT_TYPE << "). Exiting..." << std::endl;
      exit(-1);
    }
  }

  // Similarity adjustment method defines the way we apply the function reference 
  // modifier when a tag similarity against a function is being calculated.
  switch (SIMILARITY_ADJUSTMENT_METHOD) {
    case SIMILARITY_ADJUSTMENT_METHOD_ID__ADD: {
      // When adjusting similarity calculation, do so by adding function reference modifier.
      eval_hw->SetBaseFuncRefMod(0.0);
      eval_hw->SetFuncRefModifier([](double base_sim, const function_t & function) {
        return base_sim + function.GetRefModifier();
      });
      break;
    }
    case SIMILARITY_ADJUSTMENT_METHOD_ID__MULT: {
      // When adjusting similarity calculation, do so by multiplying function reference modifier.
      eval_hw->SetBaseFuncRefMod(1.0);
      eval_hw->SetFuncRefModifier([](double base_sim, const function_t & function) {
        return base_sim * function.GetRefModifier();
      });
      break;
    }
    default: {
      std::cout << "Unrecognized SIMILARITY_ADJUSTMENT_METHOD (" << SIMILARITY_ADJUSTMENT_METHOD << "). Exiting..." << std::endl;
      exit(-1);
    }
  }

  // Setup the event library
  // TODO: setup events

  // Configure hardware settings
  eval_hw->SetMinBindThresh(SGP_HW_MIN_BIND_THRESH);
  eval_hw->SetMaxCores(SGP_HW_MAX_CORES);
  eval_hw->SetMaxCallDepth(SGP_HW_MAX_CALL_DEPTH);

}

void Experiment::DoConfig__Experiment() {
  // Make a data directory.
  mkdir(DATA_DIRECTORY.c_str(), ACCESSPERMS);
  if (DATA_DIRECTORY.back() != '/') DATA_DIRECTORY += '/';

  // Configure the world.
  world->Reset();
  world->SetWellMixed(true);
  world->SetFitFun([this](agent_t & agent) { return this->CalcFitness(agent); });
  world->SetMutFun([this](agent_t & agent, emp::Random & rnd) { return this->Mutate(agent, rnd); });

  // Configure run/eval signals.
  do_pop_init_sig.AddAction([this]() { this->InitPopulation__FromAncestorFile(); });
  do_pop_snapshot_sig.AddAction([this](size_t u) { this->Snapshot__Programs(u); });

  do_begin_run_setup_sig.AddAction([this]() {
    // Do one-time run setup. (things that need to happen once, right before the run)
    std::cout << "Doing initial run setup!" << std::endl;
    // Setup systematics/fitness tracking!
    auto & sys_file = world->SetupSystematicsFile(DATA_DIRECTORY + "systematics.csv");
    sys_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
    auto & fit_file = world->SetupFitnessFile(DATA_DIRECTORY + "fitness.csv");
    fit_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
    // TODO: add dominant file back in!
    // auto & dom_file = this->AddDominantFile(DATA_DIRECTORY + "dominant.csv");
    // dom_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
    // dom_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
    // Initialize the population. 
    do_pop_init_sig.Trigger();
  });

  do_evaluation_sig.AddAction([this]() {
    // Do whole-population (single-generation) evaluation.
    double best_score = MIN_POSSIBLE_SCORE;
    dom_agent_id = 0;
    
    // Set switch times for this generation.
    reset_signal_mapping_change_trial_sig.Trigger();

    for (size_t id = 0; id < world->GetSize(); ++id) {
      // Load and configure agent.
      agent_t & our_hero = world->GetOrg(id);
      our_hero.SetID(id);
      // Take care of one-time stuff for this evaluation.
      eval_hw->SetProgram(our_hero.GetGenome());
      // Evaluate!
      this->Evaluate(our_hero);
      // TODO: configure representative phenotype!
      double score = CalcFitness(our_hero);
      if (score > best_score) { best_score = score; dom_agent_id = id; }
    }

    std::cout << "Update: " << update << " Max score: " << best_score << std::endl;
    if (update % POP_SNAPSHOT_INTERVAL == 0) do_pop_snapshot_sig.Trigger(update);

  });

  do_world_update_sig.AddAction([this]() {
    world->Update();
    world->DoMutations(ELITE_SELECT__ELITE_CNT);
  });

  begin_agent_eval_sig.AddAction([this](agent_t & agent) {

  });

  end_agent_eval_sig.AddAction([this](agent_t & agent) {

  });

  begin_agent_trial_sig.AddAction([this](agent_t & agent) {

  });

  end_agent_trial_sig.AddAction([this](agent_t & agent) {

  });

  do_agent_trial_sig.AddAction([this](agent_t & agent) {

  });

  do_agent_advance_sig.AddAction([this](agent_t & agent) {
    eval_hw->SingleProcess();
  });

  after_agent_action_sig.AddAction([this](agent_t & agent) {

  });

  // Setup the timing for changing signal-response mappings. 
  switch (SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD) {
    case SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD_ID__RAND: {
      reset_signal_mapping_change_trial_sig.AddAction([this]() {
        for (size_t i = 0; i < EVALUATION_CNT; ++i) {
          switch_trial_by_eval[i] = random->GetUInt(SIGNAL_RESPONSE_MAPPING_CHANGE_MIN_TRIAL, SIGNAL_RESPONSE_MAPPING_CHANGE_MAX_TRIAL);
        }
      });    
      break;
    }
    case SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD_ID__HALF: {
      // Set mapping change times and never reset during a run.
      for (size_t i = 0; i < EVALUATION_CNT; ++i) {
        switch_trial_by_eval[i] = (size_t)(TRIAL_CNT/2);
      }
      break;
    }
    default: {
      std::cout << "Unrecognized SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD (" << SIGNAL_RESPONSE_MAPPING_CHANGE_METHOD << "). Exiting..." << std::endl;
      exit(-1);
    }
  }

  // Configure selection
  switch(SELECTION_METHOD) {
    case SELECTION_METHOD_ID__TOURNAMENT: {
      do_selection_sig.AddAction([this]() {
        emp::EliteSelect(*world, ELITE_SELECT__ELITE_CNT, 1);
        emp::TournamentSelect(*world, TOURNAMENT_SIZE, POP_SIZE - ELITE_SELECT__ELITE_CNT);
      });
      break;
    }
    default: {
      std::cout << "Unrecognized selection method (" << SELECTION_METHOD << "). Exiting..." << std::endl;
      exit(-1);
    }
  }

}

void Experiment::DoConfig__Analysis() {
  // TODO: implement analysis mode...
  std::cout << "Analysis mode hasn't been implemented yet! Exiting..." << std::endl;
  exit(-1);
}

#endif