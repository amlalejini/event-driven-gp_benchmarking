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

#include "../../utility_belt/source/utilities.h"

#include "t_maze-config.h"
#include "TMaze.h"

// Globals
constexpr size_t RUN_ID__EXP = 0;
constexpr size_t RUN_ID__ANALYSIS = 1;

constexpr size_t SIMILARITY_ADJUSTMENT_METHOD_ID__ADD = 0;
constexpr size_t SIMILARITY_ADJUSTMENT_METHOD_ID__MULT = 1;

constexpr size_t REF_MOD_ADJUSTMENT_TYPE_ID__ADD = 0;
constexpr size_t REF_MOD_ADJUSTMENT_TYPE_ID__MULT = 1;

constexpr size_t SELECTION_METHOD_ID__TOURNAMENT = 0;

constexpr size_t MAZE_TRIAL_EXECUTION_METHOD_ID__CONTINUOUS = 0;
constexpr size_t MAZE_TRIAL_EXECUTION_METHOD_ID__STEPS = 1;

constexpr size_t MAZE_CELL_TAG_GENERATION_METHOD_ID__RAND = 0;
constexpr size_t MAZE_CELL_TAG_GENERATION_METHOD_ID__LOAD = 1;

constexpr size_t TAG_WIDTH = 16;

constexpr size_t TRAIT_ID__LOC = 0;
constexpr size_t TRAIT_ID__FACING = 1;
constexpr size_t TRAIT_ID__LAST_ACTION = 2;
constexpr size_t TRAIT_ID__REWARD_FB = 3;
constexpr size_t TRAIT_ID__PENALTY_FB = 4;
constexpr size_t TRAIT_ID__REWARD_COLLECTED = 5; 
constexpr size_t TRAIT_ID__DONE = 6;

constexpr size_t ACTION_ID__NONE = 0;
constexpr size_t ACTION_ID__FORWARD = 1;
constexpr size_t ACTION_ID__ROT_CW = 2;
constexpr size_t ACTION_ID__ROT_CCW = 3;

constexpr size_t EVENT_DATA_ID__VALUE = 0;
constexpr size_t EVENT_DATA_ID__PENALTY_FB = 1;

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
    size_t total_collisions;
    size_t total_maze_completions;
    size_t total_resource_collections;
    double total_collected_resource_value;
    double total_penalty_value;
    size_t total_rotcw;
    size_t total_rotccw;
    size_t total_forward;
    size_t total_actions;

    double score;

    Phenotype()
      : total_collisions(0),
        total_maze_completions(0),
        total_resource_collections(0),
        total_collected_resource_value(0),
        total_penalty_value(0),
        total_rotcw(0),
        total_rotccw(0),
        total_forward(0),
        total_actions(0),
        score(0)
    { ; }

    size_t GetTotalCollisions() const { return total_collisions; }
    size_t GetTotalMazeCompletions() const { return total_maze_completions; }
    size_t GetTotalResourceCollections() const { return total_resource_collections; }
    double GetTotalCollectedResourceValue() const { return total_collected_resource_value; }
    double GetTotalPenaltyValue() const { return total_penalty_value; }
    size_t GetTotalRotCW() const { return total_rotcw; }
    size_t GetTotalRotCCW() const { return total_rotccw; }
    size_t GetTotalForward() const { return total_forward; }
    size_t GetTotalActions() const { return total_actions; }
    double GetScore() const { return score; }

    void Reset() {
      total_collisions = 0;
      total_maze_completions = 0;
      total_resource_collections = 0;
      total_collected_resource_value = 0;
      total_penalty_value = 0;
      total_rotcw = 0;
      total_rotccw = 0;
      total_forward = 0;
      total_actions = 0;
      score = 0;
    }

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
  // - Selection group
  size_t SELECTION_METHOD;
  size_t TOURNAMENT_SIZE;
  size_t ELITE_SELECT__ELITE_CNT;
 // - Evaluation group
  size_t EVALUATION_CNT;
  size_t MAZE_TRIAL_CNT;
  size_t MAZE_TRIAL_EXECUTION_METHOD;
  bool AFTER_ACTION__RESET;
  bool AFTER_ACTION__WIPE_SHARED_MEM;
  bool AFTER_ACTION__CLEAR_FUNC_REF_MODS;
  bool AFTER_ACTION__SIGNAL;
  bool AFTER_MAZE_TRIAL__WIPE_SHARED_MEM;
  bool AFTER_MAZE_TRIAL__CLEAR_FUNC_REF_MODS;
  bool POLLING_SENSORS;
  size_t MAZE_TRIAL_STEPS;
  size_t TIME_PER_ACTION;
  size_t MAZE_TRIAL_TIME;
  double COLLISION_PENALTY;
  double MAZE_INCOMPLETE_PENALTY;
  // - T-maze group
  size_t MAZE_CORRIDOR_LEN;
  double MAZE_SMALL_REWARD_VALUE;
  double MAZE_LARGE_REWARD_VALUE;
  size_t MAZE_CELL_TAG_GENERATION_METHOD;
  std::string MAZE_CELL_TAG_FPATH;
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
  size_t maze_trial_id; ///< Current maze trial within a single evaluation. 
  
  size_t trial_time; ///< Current time within a maze_trial. 
  size_t trial_step; ///< Current 'action step' of trial. 

  // bool performed_;
  // bool done_trial;

  size_t dom_agent_id;  ///< ID of the best agent found so far. (only meaningful during/at end of evaluating entire population)

  phen_cache_t phen_cache;

  TMaze maze;

  // std::unordered_map<TMaze::CellType, tag_t> maze_tags;
  emp::vector<tag_t> maze_tags;

  // Run signals
  emp::Signal<void(void)> do_begin_run_setup_sig;   ///< Triggered at begining of run.
  emp::Signal<void(void)> do_pop_init_sig;          ///< Triggered during run setup. Defines way population is initialized.
  
  emp::Signal<void(void)> do_evaluation_sig;        ///< Triggered during run step. Should trigger population-wide agent evaluation.
  emp::Signal<void(void)> do_selection_sig;         ///< Triggered during run step. Should trigger selection (which includes selection, reproduction, and mutation).
  emp::Signal<void(void)> do_world_update_sig;      ///< Triggered during run step. Should trigger world->Update(), and whatever else should happen right before/after population turnover.
  emp::Signal<void(void)> do_analysis_sig;          ///< Triggered for analysis mode. 

  // Systematics signals
  emp::Signal<void(size_t)> do_pop_snapshot_sig;      ///< Triggered if we should take a snapshot of the population (as defined by POP_SNAPSHOT_INTERVAL). Should call appropriate functions to take snapshot.
  // emp::Signal<void(agent_t &)> record_cur_phenotype_sig;  ///< Triggered at end of agent evaluation. Should do anything necessary to record agent phenotype.
  
  // Evaluation signals
  emp::Signal<void(agent_t &)> begin_agent_eval_sig;  ///< Triggered at beginning of agent evaluation (might be multiple trials)
  emp::Signal<void(agent_t &)> end_agent_eval_sig;  ///< Triggered at beginning of agent evaluation (might be multiple trials)

  emp::Signal<void(agent_t &)> begin_agent_maze_trial_sig; ///< Triggered at the beginning of an agent trial.
  emp::Signal<void(agent_t &)> do_agent_maze_trial_sig; ///< Triggered at the beginning of an agent trial.
  emp::Signal<void(agent_t &)> end_agent_maze_trial_sig; ///< Triggered at the beginning of an agent trial.

  emp::Signal<void(agent_t &)> maze_location_sig; ///< When triggered, give SignalGP agent event/signal indicating current location in grid.
  emp::Signal<void(agent_t &)> do_agent_advance_sig; ///< When triggered, advance SignalGP evaluation hardware
  emp::Signal<void(agent_t &)> after_agent_action_sig; ///< Triggered after agent performs action

  void Evaluate(agent_t & agent) {
    for (eval_id = 0; eval_id < EVALUATION_CNT; ++eval_id) {
      begin_agent_eval_sig.Trigger(agent);
      for (maze_trial_id = 0; maze_trial_id < MAZE_TRIAL_CNT; ++maze_trial_id) {
        begin_agent_maze_trial_sig.Trigger(agent);
        do_agent_maze_trial_sig.Trigger(agent);
        end_agent_maze_trial_sig.Trigger(agent);
      }
      end_agent_eval_sig.Trigger(agent);
    }
  }

  /// Scratch/test function.
  /// This function exists to test experiment implementation as I code it up.
  void Test() {
    std::cout << "Testing experiment!" << std::endl;

    std::cout << "Loading test program!" << std::endl;
    program_t test_prog(inst_lib);
    std::ifstream test_prog_fstream(ANCESTOR_FPATH);
    if (!test_prog_fstream.is_open()) {
      std::cout << "Failed to open test program file (" << ANCESTOR_FPATH << ")!" << std::endl;
      exit(-1);
    }
    test_prog.Load(test_prog_fstream);
    std::cout << " --- Test program: --- " << std::endl;
    test_prog.PrintProgramFull();
    std::cout << " --------------------- " << std::endl;
    Agent test_hero(test_prog);

    do_agent_advance_sig.AddAction([this](agent_t & agent) {
      std::cout << "=== T: " << trial_time << " ===" << std::endl;
      std::cout << "Function modifiers:";
      for (size_t fID = 0; fID < eval_hw->GetProgram().GetSize(); ++fID) {
        std::cout << " " << fID << ":" << eval_hw->GetProgram()[fID].GetRefModifier(); 
      } std::cout << "\n";
      eval_hw->PrintState();
    });

    // 2) Run program!
    maze.ResetRewards();
    maze.SwitchRewards();
    eval_hw->SetProgram(test_prog);
    eval_hw->ResetHardware();

    begin_agent_maze_trial_sig.Trigger(test_hero);

    // - Print hardware state
    std::cout << "=== INITIAL STATE ===" << std::endl;
    eval_hw->PrintState();
    
    do_agent_maze_trial_sig.Trigger(test_hero);
    
    end_agent_maze_trial_sig.Trigger(test_hero);

    std::cout << "=== MAZE ===" << std::endl;
    maze.Print();

    exit(-1);
  }

public:

  Experiment(const TMazeConfig & config) 
    : update(0), eval_id(0), maze_trial_id(0),
      trial_time(0), trial_step(0), 
      // done_step(false), done_trial(false),
      dom_agent_id(0), phen_cache(0, 0),
      maze(), maze_tags(0)
  { 
    // Load configuration parameters. 
    // - General parameters
    RUN_MODE = config.RUN_MODE();
    RANDOM_SEED = config.RANDOM_SEED();
    POP_SIZE = config.POP_SIZE();
    GENERATIONS = config.GENERATIONS();
    ANCESTOR_FPATH = config.ANCESTOR_FPATH();
    // - Selection parameters
    SELECTION_METHOD = config.SELECTION_METHOD();
    TOURNAMENT_SIZE = config.TOURNAMENT_SIZE();
    ELITE_SELECT__ELITE_CNT = config.ELITE_SELECT__ELITE_CNT();
    // - Evaluation parameters
    EVALUATION_CNT = config.EVALUATION_CNT();
    MAZE_TRIAL_EXECUTION_METHOD = config.MAZE_TRIAL_EXECUTION_METHOD();
    MAZE_TRIAL_CNT = config.MAZE_TRIAL_CNT();
    AFTER_ACTION__RESET = config.AFTER_ACTION__RESET();
    AFTER_ACTION__WIPE_SHARED_MEM = config.AFTER_ACTION__WIPE_SHARED_MEM();
    AFTER_ACTION__CLEAR_FUNC_REF_MODS = config.AFTER_ACTION__CLEAR_FUNC_REF_MODS();
    AFTER_ACTION__SIGNAL = config.AFTER_ACTION__SIGNAL();
    AFTER_MAZE_TRIAL__WIPE_SHARED_MEM = config.AFTER_MAZE_TRIAL__WIPE_SHARED_MEM();
    AFTER_MAZE_TRIAL__CLEAR_FUNC_REF_MODS = config.AFTER_MAZE_TRIAL__CLEAR_FUNC_REF_MODS();
    POLLING_SENSORS = config.POLLING_SENSORS();
    MAZE_TRIAL_STEPS = config.MAZE_TRIAL_STEPS();
    TIME_PER_ACTION = config.TIME_PER_ACTION();
    MAZE_TRIAL_TIME = config.MAZE_TRIAL_TIME();
    COLLISION_PENALTY = config.COLLISION_PENALTY();
    MAZE_INCOMPLETE_PENALTY = config.MAZE_INCOMPLETE_PENALTY();
    // - T-maze parameters
    MAZE_CORRIDOR_LEN = config.MAZE_CORRIDOR_LEN();
    MAZE_SMALL_REWARD_VALUE = config.MAZE_SMALL_REWARD_VALUE();
    MAZE_LARGE_REWARD_VALUE = config.MAZE_LARGE_REWARD_VALUE();
    MAZE_CELL_TAG_GENERATION_METHOD = config.MAZE_CELL_TAG_GENERATION_METHOD();
    MAZE_CELL_TAG_FPATH = config.MAZE_CELL_TAG_FPATH();
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

    if (EVALUATION_CNT < 1) {
      std::cout << "Cannot run experiment with EVALUATION_CNT < 1. Exiting..." << std::endl;
      exit(-1);
    }

    // Configure the maze.
    maze.Resize(MAZE_CORRIDOR_LEN);
    maze.SetLargeRewardValue(MAZE_LARGE_REWARD_VALUE);
    maze.SetSmallRewardValue(MAZE_SMALL_REWARD_VALUE);

    // Configure maze tags
    switch (MAZE_CELL_TAG_GENERATION_METHOD) {
      case MAZE_CELL_TAG_GENERATION_METHOD_ID__RAND: {
        maze_tags = toolbelt::GenerateRandomTags<TAG_WIDTH>(*random, TMaze::NUM_CELL_TYPES, true);
        SaveMazeTags();
        break;
      }
      case MAZE_CELL_TAG_GENERATION_METHOD_ID__LOAD: {
        GenerateMazeTags__FromTagFile();
        break;
      }
      default: {
        std::cout << "Unrecognized MAZE_CELL_TAG_GENERATION_METHOD (" << MAZE_CELL_TAG_GENERATION_METHOD << "). Exiting..." << std::endl;
        exit(-1);
      }
    }

    // Print maze tags
    std::cout << "Maze tags: " << std::endl;
    for (size_t i = 0; i < maze_tags.size(); ++i) {
      TMaze::CellType type = TMaze::GetCellType(i);
      std::cout << type << ":" << TMaze::CellTypeToString(type) << ":"; maze_tags[i].Print(); std::cout << std::endl;
    }

    // Make empty instruction/event libraries
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();
    eval_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);

    // Configure hardware and instruction/event libraries.
    DoConfig__Hardware();

    switch (RUN_MODE) {
      case RUN_ID__EXP: {
        DoConfig__Experiment();
        break;
      }
      case RUN_ID__ANALYSIS: {
        DoConfig__Analysis();
        break;
      }
    }

    // Test();
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

  void GenerateMazeTags__FromTagFile();
  void SaveMazeTags();

  // === Extra SignalGP instruction definitions ===
  // -- Execution control instructions --
  static void Inst_Call(hardware_t & hw, const inst_t & inst);      
  static void Inst_Fork(hardware_t & hw, const inst_t & inst);      
  static void Inst_Terminate(hardware_t & hw, const inst_t & inst); 
  // -- Movement instructions --
  void Inst_Forward(hardware_t & hw, const inst_t & inst);
  void Inst_RotCW(hardware_t & hw, const inst_t & inst);
  void Inst_RotCCW(hardware_t & hw, const inst_t & inst);
  // -- Sensor instructions --
  void Inst_GetCorridorLen(hardware_t & hw, const inst_t & inst);

  // === SignalGP event handlers/dispatchers ===
  static void EventDispatch__MazeLocation(hardware_t & hw, const event_t & event);
  static void EventHandler__MazeLocation(hardware_t & hw, const event_t & event);

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

void Experiment::Inst_RotCW(hardware_t & hw, const inst_t & inst) {
  if (hw.GetTrait(TRAIT_ID__LAST_ACTION)) return; // Not allowed to do two actions per time step.
  hw.SetTrait(TRAIT_ID__FACING, emp::Mod(hw.GetTrait(TRAIT_ID__FACING) + 1, TMaze::NUM_DIRECTIONS));
  hw.SetTrait(TRAIT_ID__LAST_ACTION, ACTION_ID__ROT_CW);
}

void Experiment::Inst_RotCCW(hardware_t & hw, const inst_t & inst) {
  if (hw.GetTrait(TRAIT_ID__LAST_ACTION)) return; // Not allowed to do two actions per time step.
  hw.SetTrait(TRAIT_ID__FACING, emp::Mod(hw.GetTrait(TRAIT_ID__FACING) - 1, TMaze::NUM_DIRECTIONS));
  hw.SetTrait(TRAIT_ID__LAST_ACTION, ACTION_ID__ROT_CCW);
}

void Experiment::Inst_Forward(hardware_t & hw, const inst_t & inst) {
  if (hw.GetTrait(TRAIT_ID__LAST_ACTION)) return; // Not allowed to do two actions per time step.
  const TMaze::Facing facing = TMaze::GetFacing(eval_hw->GetTrait(TRAIT_ID__FACING));
  const size_t maze_loc = eval_hw->GetTrait(TRAIT_ID__LOC);
  const TMaze::Cell & cur_cell = maze.GetCell(maze_loc);

  // Possibilities when moving foward:
  // 1) There is a neighboring cell in the agent's current facing.
  //  - In this case, the movement succeeds, and we update the agent's current location. 
  // 2) There is not a neighboring cell in the agent's current facing. 
  //  - In this case, the movement fails, and the agent collides w/the wall (incurring a penalty). 
  
  if (cur_cell.HasNeighbor(facing)) { 
    // These is a cell to be moved to. 
    hw.SetTrait(TRAIT_ID__LOC, cur_cell.GetNeighborID(facing));
  } else {
    // Collision!
    hw.SetTrait(TRAIT_ID__PENALTY_FB, 1);
  }
  hw.SetTrait(TRAIT_ID__LAST_ACTION, ACTION_ID__FORWARD);
}

void Experiment::Inst_GetCorridorLen(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  state.SetLocal(inst.args[0], MAZE_CORRIDOR_LEN);
}

// ================== Event definition implementations ==================
void Experiment::EventDispatch__MazeLocation(hardware_t & hw, const event_t & event) {
  hw.QueueEvent(event); // self-queue the event
}

void Experiment::EventHandler__MazeLocation(hardware_t & hw, const event_t & event) {
  hw.SpawnCore(event.affinity, hw.GetMinBindThresh(), event.msg, false, true);
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

void Experiment::GenerateMazeTags__FromTagFile() {
  maze_tags.resize(TMaze::NUM_CELL_TYPES, tag_t());
  std::ifstream tag_fstream(MAZE_CELL_TAG_FPATH);
  if (!tag_fstream.is_open()) {
    std::cout << "Failed to open env_tags.csv. Exiting..." << std::endl;
    exit(-1);
  }
  std::string cur_line;
  emp::vector<std::string> line_components;
  std::getline(tag_fstream, cur_line); // Consume header.
  while (!tag_fstream.eof()) {
    std::getline(tag_fstream, cur_line);
    emp::remove_whitespace(cur_line);
    if (cur_line == emp::empty_string()) continue;
    emp::slice(cur_line, line_components, ',');
    int tag_id = std::stoi(line_components[0]);
    if (tag_id > maze_tags.size()) {
      std::cout << "WARNING: tag ID exceeds NUM_CELL_TYPES" << std::endl;
      continue;
    }
    for (size_t i = 0; i < line_components[1].size(); ++i) {
      if (i >= TAG_WIDTH) break;
      if (line_components[1][i] == '1') maze_tags[tag_id].Set(maze_tags[tag_id].GetSize() - i - 1, true);
    }
  }
  tag_fstream.close();
}

void Experiment::SaveMazeTags() {
  // Save out the environment states.
  std::ofstream mazetags_ofstream(MAZE_CELL_TAG_FPATH);
  mazetags_ofstream << "cell_id,tag\n";
  for (size_t i = 0; i < maze_tags.size(); ++i) {
    mazetags_ofstream << i << ","; maze_tags[i].Print(mazetags_ofstream); mazetags_ofstream << "\n";
  }
  mazetags_ofstream.close();
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
  double score = phen_cache.Get(aID, 0).GetScore();
  // Return the minimum score!
  for (size_t eID = 1; eID < EVALUATION_CNT; ++eID) {
    phenotype_t & phen = phen_cache.Get(aID, eID);
    if (phen.GetScore() < score) score = phen.GetScore();
  }
  return score;
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

  // Actuation instructions
  // - Forward
  inst_lib->AddInst("Forward", [this](hardware_t & hw, const inst_t & inst) {
    this->Inst_Forward(hw, inst);
  }, 0, "If the agent can move forward, move the agent forward in the maze. Otherwise, collision!");
  // - RotCW
  inst_lib->AddInst("RotCW", [this](hardware_t & hw, const inst_t & inst) {
    this->Inst_RotCW(hw, inst);
  }, 0, "Rotate agent clockwise.");
  // - RotCCW
  inst_lib->AddInst("RotCCW", [this](hardware_t & hw, const inst_t & inst) {
    this->Inst_RotCCW(hw, inst);
  }, 0, "Rotate agent counter-clockwise.");

  // Sensors
  inst_lib->AddInst("GetCorridorLen", [this](hardware_t & hw, const inst_t & inst) {
    this->Inst_GetCorridorLen(hw, inst);
  }, 1, "WM[ARG0] = CORRIDOR LENGTH");

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

  // Setup the event library.
  event_lib->AddEvent("MazeLocation", EventHandler__MazeLocation, "Maze location event. Triggered when agent moves onto new location.");
  event_lib->RegisterDispatchFun("MazeLocation", EventDispatch__MazeLocation);

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
    // TODO: snapshot here!

  });
  
    // do_world_update_sig
  do_world_update_sig.AddAction([this]() {
    world->Update();
    world->DoMutations(ELITE_SELECT__ELITE_CNT);
  });

  // NOTE: there may be multiple evaluations for each agent.
  begin_agent_eval_sig.AddAction([this](agent_t & agent) {
    // Reset hardware (hard, complete reset)
    eval_hw->ResetHardware();
    // Reset the maze 
    maze.RandomizeRewards(*random);
    // Reset phenotype.
    const size_t agentID = agent.GetID();
    phenotype_t & phen = phen_cache.Get(agentID, eval_id);
    phen.Reset();
  });

  begin_agent_maze_trial_sig.AddAction([this](agent_t & agent) {
    
    // Reset maze
    maze.ResetRewards();

    // Reset hardware, but...
    //  - Do we wipe shared memory between trials? 
    //  - Do we reset function reference modifiers between trials?
    eval_hw->ResetHardware(AFTER_MAZE_TRIAL__WIPE_SHARED_MEM, AFTER_MAZE_TRIAL__CLEAR_FUNC_REF_MODS);

    TMaze::Cell & start_cell = maze.GetCell(maze.GetStartCellID());

    // Configure traits for trail.    
    eval_hw->SetTrait(TRAIT_ID__LOC, maze.GetStartCellID());  // Set location trait.
    eval_hw->SetTrait(TRAIT_ID__FACING, TMaze::GetFacing(TMaze::Facing::N)); // Set heading trait.
    eval_hw->SetTrait(TRAIT_ID__REWARD_FB, 0);
    eval_hw->SetTrait(TRAIT_ID__PENALTY_FB, 0);
    eval_hw->SetTrait(TRAIT_ID__REWARD_COLLECTED, 0);
    eval_hw->SetTrait(TRAIT_ID__DONE, 0);
    
    // Trigger START signal
    maze_location_sig.Trigger(agent);
    memory_t mem;
    mem[EVENT_DATA_ID__VALUE] = start_cell.GetValue();
    mem[EVENT_DATA_ID__PENALTY_FB] = 0;
    eval_hw->TriggerEvent("MazeLocation", maze_tags[TMaze::GetCellType(TMaze::CellType::START)], mem);

  });

  end_agent_maze_trial_sig.AddAction([this](agent_t & agent) {
    // If you end the trial w/out completing the maze, suffer!
    if (!eval_hw->GetTrait(TRAIT_ID__DONE)) {
      const size_t agentID = agent.GetID();
      phenotype_t & phen = phen_cache.Get(agentID, eval_id);
      phen.total_penalty_value += MAZE_INCOMPLETE_PENALTY;
    }
  });

  // Configure trial execution
  switch (MAZE_TRIAL_EXECUTION_METHOD) {
    case MAZE_TRIAL_EXECUTION_METHOD_ID__CONTINUOUS: {
      do_agent_maze_trial_sig.AddAction([this](agent_t & agent) {
        // Do the trial!
        for (trial_time = 0; trial_time < MAZE_TRIAL_TIME; ++trial_time) {
          do_agent_advance_sig.Trigger(agent);
          if (eval_hw->GetTrait(TRAIT_ID__LAST_ACTION)) {
            after_agent_action_sig.Trigger(agent);
            if (eval_hw->GetTrait(TRAIT_ID__DONE)) break;
          }
        }
      });
      break;
    }
    case MAZE_TRIAL_EXECUTION_METHOD_ID__STEPS: {
      do_agent_maze_trial_sig.AddAction([this](agent_t & agent) {
        for (trial_step = 0; trial_step < MAZE_TRIAL_STEPS; ++trial_step) { 
          // Run step until action or until step-time runs out
          for (trial_time = 0; trial_time < TIME_PER_ACTION; ++trial_time) {
            do_agent_advance_sig.Trigger(agent); 
            if (eval_hw->GetTrait(TRAIT_ID__LAST_ACTION)) { break; }
          } // End single step
          after_agent_action_sig.Trigger(agent);
          if (eval_hw->GetTrait(TRAIT_ID__DONE)) break;
        } // End trial 
      });
      break;
    }
    default: {
      std::cout << "Unrecognized maze trial execution method (" << MAZE_TRIAL_EXECUTION_METHOD << "). Exiting..." << std::endl;
      exit(-1);
    }
  }

  do_agent_advance_sig.AddAction([this](agent_t & agent) {
    // Advance the agent
    eval_hw->SingleProcess();
  });

  // Gets triggered when an agent goes to a new location... TODO: finish, incorporate memory wiping, etc
  maze_location_sig.AddAction([this](agent_t & agent) {
    // Get the phenotype (to be adjusted)
    const size_t agentID = agent.GetID();
    phenotype_t & phen = phen_cache.Get(agentID, eval_id);

    // Where is the agent at? 
    const size_t loc = (size_t)eval_hw->GetTrait(TRAIT_ID__LOC);
    TMaze::Cell & cell = maze.GetCell(loc);
    const size_t type_id = TMaze::GetCellType(cell.GetType());
    const double cell_value = cell.GetValue();
    const size_t last_action_id = (size_t)eval_hw->GetTrait(TRAIT_ID__LAST_ACTION);

    // Did agent collect a reward?
    if (cell.GetType() == TMaze::CellType::REWARD) {
      eval_hw->SetTrait(TRAIT_ID__REWARD_FB, cell_value);
      eval_hw->SetTrait(TRAIT_ID__REWARD_COLLECTED, 1);
      phen.total_resource_collections++;
      phen.total_collected_resource_value += cell_value;
      maze.ClearRewards();
    }

    // Did the agent finish the maze?
    if (cell.GetType() == TMaze::CellType::START && eval_hw->GetTrait(TRAIT_ID__REWARD_COLLECTED)) {
      eval_hw->SetTrait(TRAIT_ID__DONE, 1);
      phen.total_maze_completions++;
    }

    // Gather agent's reward/penalty feedback
    const double penalty_fb = eval_hw->GetTrait(TRAIT_ID__PENALTY_FB);
    const double reward_fb = eval_hw->GetTrait(TRAIT_ID__REWARD_FB); 

    // Incur penalty for collision!
    if (penalty_fb > 0) {
      phen.total_collisions++;
      phen.total_penalty_value += COLLISION_PENALTY;
    }
    
    switch (last_action_id) {
      case ACTION_ID__NONE: { break; }
      case ACTION_ID__FORWARD: { phen.total_forward++; break; }
      case ACTION_ID__ROT_CW: { phen.total_rotcw++; break; }
      case ACTION_ID__ROT_CCW: { phen.total_rotccw++; break; }
      default: {
        std::cout << "Unrecognized action! Something has gone horribly wrong! Exiting..." << std::endl;
        exit(-1);
      }
    }
    phen.total_actions++;   
     
  });

  after_agent_action_sig.AddAction([this](agent_t & agent) {
    // Trigger maze location
    maze_location_sig.Trigger(agent);
        
    if (AFTER_ACTION__RESET) {
      eval_hw->ResetHardware(AFTER_ACTION__WIPE_SHARED_MEM, AFTER_ACTION__CLEAR_FUNC_REF_MODS);
    }

    if (AFTER_ACTION__SIGNAL) {
      TMaze::Cell & cell = maze.GetCell(eval_hw->GetTrait(TRAIT_ID__LOC));
      memory_t mem;
      mem[EVENT_DATA_ID__VALUE] = cell.GetValue();
      mem[EVENT_DATA_ID__PENALTY_FB] = eval_hw->GetTrait(TRAIT_ID__PENALTY_FB);
      eval_hw->TriggerEvent("MazeLocation", maze_tags[TMaze::GetCellType(cell.GetType())], mem);
    }

    // After action clean-up
    eval_hw->SetTrait(TRAIT_ID__LAST_ACTION, ACTION_ID__NONE);
    eval_hw->SetTrait(TRAIT_ID__PENALTY_FB, 0);
    eval_hw->SetTrait(TRAIT_ID__REWARD_FB, 0);
        
  });

  end_agent_eval_sig.AddAction([this](agent_t & agent) {
    const size_t agentID = agent.GetID();
    phenotype_t & phen = phen_cache.Get(agentID, eval_id);
    // End of an evalution, should go ahead and record the score for this evaluation period.
    phen.score = phen.GetTotalCollectedResourceValue() - phen.GetTotalPenaltyValue();
  });  

  // Configure selection
  switch (SELECTION_METHOD) {
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
  // TODO: implement do config analysis... 
}

#endif