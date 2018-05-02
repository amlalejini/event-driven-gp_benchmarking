#ifndef TMAZE_EXPERIMENT_H
#define TMAZE_EXPERIMENT_H

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

#include "t_maze-config.h"
#include "TMaze.h"

// Globals
constexpr size_t RUN_ID__EXP = 0;
constexpr size_t RUN_ID__ANALYSIS = 1;

constexpr size_t SIMILARITY_ADJUSTMENT_METHOD_ID__ADD = 0;
constexpr size_t SIMILARITY_ADJUSTMENT_METHOD_ID__MULT = 1;

constexpr size_t REF_MOD_ADJUSTMENT_TYPE_ID__ADD = 0;
constexpr size_t REF_MOD_ADJUSTMENT_TYPE_ID__MULT = 1;

constexpr size_t TAG_WIDTH = 16;

constexpr double MIN_POSSIBLE_SCORE = -32767;

/// Class to manage t-maze experiment. 
class Experiment {
public:
  // Forward declarations
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

  /// Phenotype of agents being evolved
  struct Phenotype {
    // TODO: Fill out phenotype struct.
  };

  class PhenotypeCache {
    protected:
      size_t agent_cnt;
      size_t eval_cnt;
      emp::vector<phenotype_t> agent_phen_cache;

    public:
      PhenotypeCache(size_t _agent_cnt, size_t _eval_cnt) 
        : agent_cnt(_agent_cnt), eval_cnt(_eval_cnt), 
          agent_phen_cache(agent_cnt * eval_cnt)
      { ; }

      /// Resize phenotype cache. 
      void Resize(size_t _agent_cnt, size_t _eval_cnt) {
        agent_cnt = _agent_cnt;
        eval_cnt = _eval_cnt;
        agent_phen_cache.clear();
        agent_phen_cache.resize(agent_cnt * eval_cnt);
      }

      /// Access a phenotype from the cache
      phenotype_t & Get(size_t agent_id, size_t eval_id) {
        return agent_phen_cache[(agent_id * eval_cnt) + eval_id];
      }

      // TODO: reset entire cache
      // void Reset() { ; }
  };

protected:
  // Configuration parameters
  // - General group
  size_t RUN_MODE;
  int RANDOM_SEED;
  size_t POP_SIZE;
  size_t GENERATIONS;
  std::string ANCESTOR_FPATH;
  size_t EVALUATION_CNT;
  // - Selection group
  size_t SELECTION_METHOD;
  size_t TOURNAMENT_SIZE;
  size_t ELITE_SELECT__ELITE_CNT;
  // - T-maze group
  size_t MAZE_CORRIDOR_LEN;
  // - SignalGP function regulation group
  size_t SIMILARITY_ADJUSTMENT_METHOD;
  size_t REF_MOD_ADJUSTMENT_TYPE;
  size_t REF_MOD_ADJUSTMENT_VALUE;
  bool MODIFY_REG;
  // - SignalGP program group
  size_t SGP_PROG_MAX_FUNC_CNT;
  size_t SGP_PROG_MIN_FUNC_CNT;
  size_t SGP_PROG_MAX_FUNC_LEN;
  size_t SGP_PROG_MIN_FUNC_LEN;
  size_t SGP_PROG_MAX_TOTAL_LEN;
  // - SignalGP hardware group
  size_t SGP_HW_MAX_CORES;
  size_t SGP_HW_MAX_CALL_DEPTH;
  double SGP_HW_MIN_BIND_THRESH;
  // - SignalGP mutation group
  size_t SGP__PROG_MAX_ARG_VAL;
  double SGP__PER_BIT__TAG_BFLIP_RATE;
  double SGP__PER_INST__SUB_RATE;
  double SGP__PER_INST__INS_RATE;
  double SGP__PER_INST__DEL_RATE;
  double SGP__PER_FUNC__SLIP_RATE;
  double SGP__PER_FUNC__FUNC_DUP_RATE;
  double SGP__PER_FUNC__FUNC_DEL_RATE;
  // - Data collection group
  size_t SYSTEMATICS_INTERVAL;
  size_t FITNESS_INTERVAL;
  size_t POP_SNAPSHOT_INTERVAL;
  std::string DATA_DIRECTORY;

  // Experiment variables
  emp::Ptr<emp::Random> random;     ///< Random number generator
  emp::Ptr<world_t> world;          ///< Empirical world for evolution

  emp::Ptr<inst_lib_t> inst_lib;    ///< SignalGP instruction library
  emp::Ptr<event_lib_t> event_lib;  ///< SignalGP event library

  emp::Ptr<hardware_t> eval_hw;     ///< SignalGP virtual hardware used for evaluation

  size_t update;    ///< Current update (generation) of experiment
  size_t eval_id;   ///< Current trial of current evaluation. (only meaningful during an agent evaluation)
  size_t eval_time; ///< Current evaluation time within a trial. (only meaningful within an trial evaluation)

  size_t dom_agent_id;  ///< ID of the best agent found so far. (only meaningful during/at end of evaluating entire population)

  phen_cache_t phen_cache;

  TMaze maze;

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
  emp::Signal<void(agent_t &)> begin_agent_trial_sig; ///< Triggered at the beginning of an agent trial.
  emp::Signal<void(agent_t &)> record_cur_phenotype_sig;  ///< Triggered at end of agent evaluation. Should do anything necessary to record agent phenotype.
  

  void Evaluate(agent_t & agent) {
    begin_agent_eval_sig.Trigger(agent);
    // ... 
  }
  
  /// Scratch/test function.
  /// This function exists to test experiment implementation as I code it up.
  void Test() {
    std::cout << "Testing experiment!" << std::endl;

    // std::cout << "Loading test program!" << std::endl;
    // program_t test_prog(inst_lib);
    // std::ifstream test_prog_fstream(ANCESTOR_FPATH);
    // if (!test_prog_fstream.is_open()) {
    //   std::cout << "Failed to open test program file (" << ANCESTOR_FPATH << ")!" << std::endl;
    //   exit(-1);
    // }
    // test_prog.Load(test_prog_fstream);
    // std::cout << " --- Test program: --- " << std::endl;
    // test_prog.PrintProgramFull();
    // std::cout << " --------------------- " << std::endl;

    // // 2) Run program!
    // eval_hw->SetProgram(test_prog);
    // eval_hw->ResetHardware();
    // eval_hw->SpawnCore(0, memory_t(), false);
    // // - Print hardware state
    // std::cout << "=== INITIAL STATE ===" << std::endl;
    // eval_hw->PrintState();
    // for (size_t t = 0; t < 32; ++t) {
    //   eval_hw->SingleProcess();
    //   std::cout << "=== T: " << t << " ===" << std::endl;
    //   // Print function modifiers
    //   std::cout << "Function modifiers:";
    //   for (size_t fID = 0; fID < eval_hw->GetProgram().GetSize(); ++fID) {
    //     std::cout << " " << fID << ":" << eval_hw->GetProgram()[fID].GetRefModifier(); 
    //   } std::cout << "\n";
    //   eval_hw->PrintState();
    // }

    std::cout << "=== MAZE ===" << std::endl;
    maze.Print();
  }
public:

  Experiment(const TMazeConfig & config) 
    : update(0), eval_id(0), eval_time(0),
      dom_agent_id(0), phen_cache(0, 0),
      maze()
  { 
    // Load configuration parameters. 
    // - General parameters
    RUN_MODE = config.RUN_MODE();
    RANDOM_SEED = config.RANDOM_SEED();
    POP_SIZE = config.POP_SIZE();
    GENERATIONS = config.GENERATIONS();
    ANCESTOR_FPATH = config.ANCESTOR_FPATH();
    EVALUATION_CNT = config.EVALUATION_CNT();
    // - Selection parameters
    SELECTION_METHOD = config.SELECTION_METHOD();
    TOURNAMENT_SIZE = config.TOURNAMENT_SIZE();
    ELITE_SELECT__ELITE_CNT = config.ELITE_SELECT__ELITE_CNT();
    // - T-maze parameters
    MAZE_CORRIDOR_LEN = config.MAZE_CORRIDOR_LEN();
    // - SignalGP function regulation parameters
    SIMILARITY_ADJUSTMENT_METHOD = config.SIMILARITY_ADJUSTMENT_METHOD();
    REF_MOD_ADJUSTMENT_TYPE = config.REF_MOD_ADJUSTMENT_TYPE();
    REF_MOD_ADJUSTMENT_VALUE = config.REF_MOD_ADJUSTMENT_VALUE();
    MODIFY_REG = config.MODIFY_REG();
    // - SignalGP program parameters
    SGP_PROG_MAX_FUNC_CNT = config.SGP_PROG_MAX_FUNC_CNT();
    SGP_PROG_MIN_FUNC_CNT = config.SGP_PROG_MIN_FUNC_CNT();
    SGP_PROG_MAX_FUNC_LEN = config.SGP_PROG_MAX_FUNC_LEN();
    SGP_PROG_MIN_FUNC_LEN = config.SGP_PROG_MIN_FUNC_LEN();
    SGP_PROG_MAX_TOTAL_LEN = config.SGP_PROG_MAX_TOTAL_LEN();
    // - SignalGP hardware parameters
    SGP_HW_MAX_CORES = config.SGP_HW_MAX_CORES();
    SGP_HW_MAX_CALL_DEPTH = config.SGP_HW_MAX_CALL_DEPTH();
    SGP_HW_MIN_BIND_THRESH = config.SGP_HW_MIN_BIND_THRESH();
    // - SignalGP mutation parameters
    SGP__PROG_MAX_ARG_VAL = config.SGP__PROG_MAX_ARG_VAL();
    SGP__PER_BIT__TAG_BFLIP_RATE = config.SGP__PER_BIT__TAG_BFLIP_RATE();
    SGP__PER_INST__SUB_RATE = config.SGP__PER_INST__SUB_RATE();
    SGP__PER_INST__INS_RATE = config.SGP__PER_INST__INS_RATE();
    SGP__PER_INST__DEL_RATE = config.SGP__PER_INST__DEL_RATE();
    SGP__PER_FUNC__SLIP_RATE = config.SGP__PER_FUNC__SLIP_RATE();
    SGP__PER_FUNC__FUNC_DUP_RATE = config.SGP__PER_FUNC__FUNC_DUP_RATE();
    SGP__PER_FUNC__FUNC_DEL_RATE = config.SGP__PER_FUNC__FUNC_DEL_RATE();
    // - Output group parameters
    SYSTEMATICS_INTERVAL = config.SYSTEMATICS_INTERVAL();
    FITNESS_INTERVAL = config.FITNESS_INTERVAL();
    POP_SNAPSHOT_INTERVAL = config.POP_SNAPSHOT_INTERVAL();
    DATA_DIRECTORY = config.DATA_DIRECTORY();

    // Create a new random number generator
    random = emp::NewPtr<emp::Random>(RANDOM_SEED);
    
    // Make the world!
    world = emp::NewPtr<world_t>(*random, "World");

    // Configure the phenotype cache. 
    phen_cache.Resize(POP_SIZE, EVALUATION_CNT); 

    // Configure the maze.
    maze.Resize(MAZE_CORRIDOR_LEN);

    // Make empty instruction/event libraries
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();
    eval_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);

    // Configure hardware and instruction/event libraries.
    DoConfig__Hardware();

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

  // === Do experiment functions ===
  void Run();
  void RunStep();

  // === Do config functions ===
  /// DoConfig__Hardware
  /// Calling this function configures the evaluation hardware based on experiment configuration settings. 
  void DoConfig__Hardware(); 
  
  /// DoConfig__Run
  /// Calling this function configures the experiment (to be run) based on experiment settings. 
  void DoConfig__Experiment();

  /// DoConfig__Analysis
  /// Calling this function configures analysis mode (to be run). 
  void DoConfig__Analysis();  
  
  // === Evolution functions ===
  size_t Mutate(agent_t & agent, emp::Random & rnd);
  double CalcFitness(agent_t & agent);

  // === Misc. utility functions ===
  void InitPopulation__FromAncestorFile();

  // === Extra SignalGP instruction definitions ===
  // -- Execution control instructions --
  static void Inst_Call(hardware_t & hw, const inst_t & inst);      
  static void Inst_Fork(hardware_t & hw, const inst_t & inst);      
  static void Inst_Terminate(hardware_t & hw, const inst_t & inst); 

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
  world->Inject(ancestor_prog, 1);    // Inject single, common ancestor into population.
}

// ================== Evolution function implementations ==================
size_t Experiment::Mutate(agent_t & agent, emp::Random & rnd) {
  program_t & program = agent.GetGenome();
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

double Experiment::CalcFitness(agent_t & agent) {
  size_t aID = agent.GetID();
  size_t eID = 0;
  // TODO: implement this! (requires me knowing what Phenotype will look like...)
  // double val = ;
  // size_t mt = 0;
  // for (size_t i = 0; i < scores_by_trial.size(); ++i) {
  //   const double trial_score = scores_by_trial[i];
  //   if (trial_score < val) { val = trial_score; mt = i; }
  // }
  // min_trial = mt;
  return 0.0;
}

// ================== Configuration implementations ==================
void Experiment::DoConfig__Hardware() {
  // Setup the instruction set
  // - Standard instructions:
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
  inst_lib->AddInst("Fork", Inst_Fork, 0, "Fork a new thread. Local memory contents of callee are loaded into forked thread's input memory.");
  inst_lib->AddInst("Terminate", Inst_Terminate, 0, "Kill current thread.");

  // TODO: more experiment-specific instructions
  
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
      }, 0, "Up regulate target function. Use tag to determine function target.");

      inst_lib->AddInst("Repress", [this](hardware_t & hw, const inst_t & inst) {
        emp::vector<size_t> targets(hw.FindBestFuncMatch(inst.affinity, 0.0, MODIFY_REG));
        size_t targetID;
        if (targets.empty()) return; 
        if (targets.size() == 1) targetID = targets[0];
        else targetID = targets[random->GetUInt(targets.size())];
        program_t & program = hw.GetProgram();
        double cur_mod = program[targetID].GetRefModifier();
        program[targetID].SetRefModifier(cur_mod - REF_MOD_ADJUSTMENT_VALUE);
      }, 0, "Down regulate target function. Use tag to determine function target.");

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
      }, 0, "Up regulate target function. Use tag to determine function target.");

      inst_lib->AddInst("Repress", [this](hardware_t & hw, const inst_t & inst) {
        emp::vector<size_t> targets(hw.FindBestFuncMatch(inst.affinity, 0.0, MODIFY_REG));
        size_t targetID;
        if (targets.empty()) return; 
        if (targets.size() == 1) targetID = targets[0];
        else targetID = targets[random->GetUInt(targets.size())];
        program_t & program = hw.GetProgram();
        double cur_mod = program[targetID].GetRefModifier();
        program[targetID].SetRefModifier(cur_mod * (1/REF_MOD_ADJUSTMENT_VALUE));
      }, 0, "Down regulate target function. Use tag to determine function target.");
      
      break;
    }
    default: {
      std::cout << "Unrecognized REF_MOD_ADJUSTMENT_TYPE (" << REF_MOD_ADJUSTMENT_TYPE << "). Exiting..." << std::endl;
      exit(-1);
    }
  }

  eval_hw->SetMinBindThresh(SGP_HW_MIN_BIND_THRESH);
  eval_hw->SetMaxCores(SGP_HW_MAX_CORES);
  eval_hw->SetMaxCallDepth(SGP_HW_MAX_CALL_DEPTH);

}

void Experiment::DoConfig__Experiment() {
  // Make a data directory. 
  mkdir(DATA_DIRECTORY.c_str(), ACCESSPERMS);
  if (DATA_DIRECTORY.back() != '/') DATA_DIRECTORY += '/';

  // Configure the world
  world->Reset();
  world->SetWellMixed(true);
  world->SetFitFun([this](agent_t & agent) { return this->CalcFitness(agent); });
  world->SetMutFun([this](agent_t & agent, emp::Random & rnd) { return this->Mutate(agent, rnd); });

  // Configure run/eval signals 
  // - On population initialization:
  do_pop_init_sig.AddAction([this]() { this->InitPopulation__FromAncestorFile(); });
  
  // - On run setup (one-time things that need to happen right before running the experiment):
  do_begin_run_setup_sig.AddAction([this]() {
    std::cout << "Doing initial run setup!" << std::endl;
    // Setup systematics/fitness tracking.
    auto & sys_file = world->SetupSystematicsFile(DATA_DIRECTORY + "systematics.csv");
    sys_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
    auto & fit_file = world->SetupFitnessFile(DATA_DIRECTORY + "fitness.csv");
    fit_file.SetTimingRepeat(FITNESS_INTERVAL);
    // TODO: add any extra fitness tracking files
    // Initialize the population. 
    do_pop_init_sig.Trigger();
  });
  
  // - On (whole-population, single-generation) evaluation:
  do_evaluation_sig.AddAction([this]() {
    double best_score = MIN_POSSIBLE_SCORE;
    dom_agent_id = 0;

    for (size_t id = 0; id < world->GetSize(); ++id) {
      // Load and configure agent
      agent_t & our_hero = world->GetOrg(id);
      our_hero.SetID(id);
      // Take care of one-time stuff for this evaluation.
      eval_hw->SetProgram(our_hero.GetGenome());
      // Evaluate!
      this->Evaluate(our_hero);
      double score = CalcFitness(our_hero);
      if (score > best_score) { best_score = score; dom_agent_id = id; }
    }

    std::cout << "Update: " << update << " Max score: " << best_score << std::endl;

  });
  
  
  // do_evaluation_sig
  // do_selection_sig
  // do_world_update_sig

  // NOTE: there may be multiple evaluations for each agent.
  begin_agent_eval_sig.AddAction([this](agent_t & agent) {
    // Reset hardware
    eval_hw->ResetHardware();
    // TODO: Reset traits  
    // ... need to know what the task looks like... 
  });
  
  // NOTE: mutate after update; snapshot before update
}

void Experiment::DoConfig__Analysis() {
  // TODO: implement do config analysis... 
}

#endif