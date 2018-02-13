// This is the main function for the NATIVE version of this project.

#include <iostream>
#include <string>
#include <deque>
#include <unordered_set>
#include <utility>
#include <fstream>
#include <sys/stat.h>
#include <algorithm>
#include <functional>

#include "base/Ptr.h"
#include "base/vector.h"

#include "config/ArgManager.h"

#include "tools/Random.h"
#include "tools/random_utils.h"
#include "tools/math.h"
#include "tools/string_utils.h"

#include "hardware/EventDrivenGP.h"
#include "Evolve/World.h"
#include "games/Mancala.h"
#include "games-config.h"

// TODO(s):
//  [x] Support SignalGP
//  [ ] Support AvidaGP
//  [x] Support Mancala
//  [x] Add more configuration to way mancala is evaluated (reset between moves vs. not)
//    [x] Reset between moves vs. no reset between moves
//    [x] Different ways of indicating move: Submit(WM[ARG1]), Submit -> MaxIndex(Output[0:5])
//  [ ] Support Othello
//     - Add base game struct with common interface for different games: MancalaGame, OthelloGame
//  [ ] Support human v AI analysis mode.

constexpr size_t TAG_WIDTH = 16;

constexpr size_t TRAIT_ID__MOVE = 0;
constexpr size_t TRAIT_ID__DONE = 1;
constexpr size_t TRAIT_ID__PLAYER_ID = 2;

// Problem IDs
constexpr size_t PROBLEM_ID__MANCALA = 0;
constexpr size_t PROBLEM_ID__OTHELLO = 1;

//
constexpr size_t MANCALA__GET_MOVE_METHOD_ID__WM = 0; ///< Look at working memory specified by submit instruction.
constexpr size_t MANCALA__GET_MOVE_METHOD_ID__OM = 1; ///< Look at output memory buffer (0:5, max val's index is move)

/// Class to contain everything relevant to running games benchmarks w/SignalGP.
class GamesExp {
public:
  // Useful type aliases.
  using hardware_t = emp::EventDrivenGP_AW<TAG_WIDTH>;
  using program_t = hardware_t::Program;
  using state_t = hardware_t::State;
  using inst_t = hardware_t::inst_t;
  using inst_lib_t = hardware_t::inst_lib_t;
  using event_t = hardware_t::event_t;
  using event_lib_t = hardware_t::event_lib_t;
  using memory_t = hardware_t::memory_t;
  using tag_t = hardware_t::affinity_t;

  /// Struct to keep track of game-playing agent.
  struct Agent {
    program_t program;
    double score;

    Agent(const program_t & _p)
      : program(_p), score(0)
    { ; }

    Agent(const Agent && in)
      : program(in.program), score(in.score)
    { ; }

    Agent(const Agent & in)
      : program(in.program), score(in.score)
    { ; }

    program_t & GetGenome() { return program; }
  };

  using world_t = emp::World<Agent>;

protected:
  // == Configurable experiment parameters ==
  // General parameters.
  bool DEBUG_MODE;
  int RANDOM_SEED;
  size_t POP_SIZE;
  size_t GENERATIONS;
  size_t EVAL_TIME;
  size_t TRIAL_CNT;
  size_t FITNESS_CALC_TYPE;
  std::string ANCESTOR_FPATH;
  size_t PROBLEM;
  bool RESET_HW_BETWEEN_MOVES;

  // Mancala parameters.
  size_t MANCALA__GET_MOVE_METHOD;

  // Hardware parameters.
  size_t HW_MAX_CORES;
  size_t HW_MAX_CALL_DEPTH;
  double HW_MIN_BIND_THRESH;

  // Mutation parameters.
  size_t PROG_MAX_FUNC_CNT;
  size_t PROG_MAX_FUNC_LEN;
  size_t PROG_MAX_ARG_VAL;
  double PER_BIT__TAG_BFLIP_RATE;
  double PER_INST__SUB_RATE;
  double PER_INST__INS_RATE;
  double PER_INST__DEL_RATE;
  double PER_FUNC__SLIP_RATE;
  double PER_FUNC__FUNC_DUP_RATE;
  double PER_FUNC__FUNC_DEL_RATE;

  // Data collection parameters.
  size_t SYSTEMATICS_INTERVAL;
  size_t POPULATION_INTERVAL;
  size_t FITNESS_INTERVAL;
  size_t POP_SNAPSHOT_INTERVAL;
  std::string DATA_DIRECTORY;
  bool RUN_FROM_EXISTING_POP;
  std::string EXISTING_POP_LOC;

  // Analysis mode parameters.
  bool ANALYZE_MODE;
  size_t ANALYSIS;
  size_t FDOM_ANALYSIS_TRIAL_CNT;
  std::string ANALYZE_AGENT_FPATH;

  // Rando member variables.
  emp::Ptr<emp::Random> random;
  emp::Ptr<world_t> world;

  emp::Ptr<inst_lib_t> inst_lib;
  emp::Ptr<event_lib_t> event_lib;

  emp::Ptr<hardware_t> eval_hw;
  emp::Ptr<hardware_t> opp_hw;

  struct MancalaGame {
    using mancala_ai_t = std::function<size_t(emp::Mancala & game)>;
    emp::Mancala mancala_game;
    std::function<size_t(const hardware_t &)> get_move;
    std::function<void(hardware_t &, const memory_t &)> reset_hw;
    size_t move_eval_time;
    bool verbose;
    size_t start_player;


    MancalaGame()
      : mancala_game(), get_move(), reset_hw(), move_eval_time(512),
        verbose(false), start_player(0)
    { ; }

    emp::Mancala & GetCurGame() { return mancala_game; }

    void SetVerbose(bool v) { verbose = v; }
    void SetStartPlayer(size_t p) { start_player = p; }

    void SetMoveEvalTime(size_t t) { move_eval_time = t; }

    /// Based on Mancala example in Empirical library (originally written by Charles Ofria)
    double EvalMancala(mancala_ai_t & player0, mancala_ai_t & player1) {
      size_t cur_player = start_player;
      mancala_game.Reset(cur_player==0);
      size_t rnd = 0, errors = 0;
      while (mancala_game.IsDone() == false) {
        // Determine the current player and their move.
        auto & play_fun = (cur_player == 0) ? player0 : player1;
        size_t best_move = play_fun(mancala_game);

        if (verbose) {
          std::cout << "round = " << rnd++ << "   errors = " << errors << std::endl;
          mancala_game.Print();
          char move_sym = (char) ('A' + best_move);
          std::cout << "Move = " << move_sym;
          if (!mancala_game.IsMoveValid(best_move)) {
            std::cout << " (illegal!)";
          }
          std::cout << std::endl << std::endl;
        }

        // If the chosen move is illegal, shift through other options.
        while (!mancala_game.IsMoveValid(best_move)) {
          if (cur_player == 0) errors++;
          if (++best_move > 5) best_move = 0;
        }

        // Do the move and determine who goes next.
        bool go_again = mancala_game.DoMove(cur_player, best_move);
        if (!go_again) cur_player = !cur_player;
      }

      if (verbose) {
        std::cout << "Final scores -- A: " << mancala_game.ScoreA()
                  << "   B: " << mancala_game.ScoreB()
        << std::endl;
      }
      return ((double)mancala_game.ScoreA()) - ((double)mancala_game.ScoreB()) - ((double)errors*10.0);
    }

    /// Based on Mancala example in Empirical library (originally written by Charles Ofria)
    double EvalMancala(hardware_t & hw0, hardware_t & hw1) {
      hw0.SetTrait(TRAIT_ID__PLAYER_ID, 0);
      hw1.SetTrait(TRAIT_ID__PLAYER_ID, 1);
      mancala_ai_t agent0_fun = [&hw0, this](emp::Mancala & game){ return this->EvalMancalaMove(game, hw0); };
      mancala_ai_t agent1_fun = [&hw1, this](emp::Mancala & game){ return this->EvalMancalaMove(game, hw1); };
      return EvalMancala(agent0_fun, agent1_fun);
    }

    size_t EvalMancalaMove(emp::Mancala & game, hardware_t & hw) {
      // Reset hardware.
      reset_hw(hw, game.AsInput(game.GetCurPlayer()));

      // Run code until time runs out or until move has been set.
      for (size_t i = 0; i < move_eval_time && !hw.GetTrait(TRAIT_ID__DONE); ++i) {
        hw.SingleProcess();
      }
      return get_move(hw);
    }
  };

  struct OthelloGame {

  };

  std::function<double(hardware_t &, hardware_t &)> eval_game;

  MancalaGame mancala;


public:
  GamesExp(const GamesConfig & config)
    : eval_game(), mancala()
  {
    // Fill out experiment parameters with config settings.
    DEBUG_MODE = config.DEBUG_MODE();
    RANDOM_SEED = config.RANDOM_SEED();
    POP_SIZE = config.POP_SIZE();
    GENERATIONS = config.GENERATIONS();
    EVAL_TIME = config.EVAL_TIME();
    TRIAL_CNT = config.TRIAL_CNT();
    FITNESS_CALC_TYPE = config.FITNESS_CALC_TYPE();
    ANCESTOR_FPATH = config.ANCESTOR_FPATH();
    PROBLEM = config.PROBLEM();
    RESET_HW_BETWEEN_MOVES = config.RESET_HW_BETWEEN_MOVES();
    MANCALA__GET_MOVE_METHOD = config.MANCALA__GET_MOVE_METHOD();
    HW_MAX_CORES = config.HW_MAX_CORES();
    HW_MAX_CALL_DEPTH = config.HW_MAX_CALL_DEPTH();
    HW_MIN_BIND_THRESH = config.HW_MIN_BIND_THRESH();
    PROG_MAX_FUNC_CNT = config.PROG_MAX_FUNC_CNT();
    PROG_MAX_FUNC_LEN = config.PROG_MAX_FUNC_LEN();
    PROG_MAX_ARG_VAL = config.PROG_MAX_ARG_VAL();
    PER_BIT__TAG_BFLIP_RATE = config.PER_BIT__TAG_BFLIP_RATE();
    PER_INST__SUB_RATE = config.PER_INST__SUB_RATE();
    PER_INST__INS_RATE = config.PER_INST__INS_RATE();
    PER_INST__DEL_RATE = config.PER_INST__DEL_RATE();
    PER_FUNC__SLIP_RATE = config.PER_FUNC__SLIP_RATE();
    PER_FUNC__FUNC_DUP_RATE = config.PER_FUNC__FUNC_DUP_RATE();
    PER_FUNC__FUNC_DEL_RATE = config.PER_FUNC__FUNC_DEL_RATE();
    SYSTEMATICS_INTERVAL = config.SYSTEMATICS_INTERVAL();
    POPULATION_INTERVAL = config.POPULATION_INTERVAL();
    FITNESS_INTERVAL = config.FITNESS_INTERVAL();
    POP_SNAPSHOT_INTERVAL = config.POP_SNAPSHOT_INTERVAL();
    DATA_DIRECTORY = config.DATA_DIRECTORY();
    RUN_FROM_EXISTING_POP = config.RUN_FROM_EXISTING_POP();
    EXISTING_POP_LOC = config.EXISTING_POP_LOC();
    ANALYZE_MODE = config.ANALYZE_MODE();
    ANALYSIS = config.ANALYSIS();
    FDOM_ANALYSIS_TRIAL_CNT = config.FDOM_ANALYSIS_TRIAL_CNT();
    ANALYZE_AGENT_FPATH = config.ANALYZE_AGENT_FPATH();

    // Setup the output directory.
    mkdir(DATA_DIRECTORY.c_str(), ACCESSPERMS);
    if (DATA_DIRECTORY.back() != '/') DATA_DIRECTORY += '/';

    // clean up existing population location directory.
    if (EXISTING_POP_LOC.back() != '/') EXISTING_POP_LOC += '/';

    // Make our random number generator.
    random = emp::NewPtr<emp::Random>(RANDOM_SEED);

    // Make the world!
    world = emp::NewPtr<world_t>(random, "Games-World");
    world->Reset();
    // Configure the world.
    world->SetWellMixed(true);
    world->SetMutFun([this](Agent & agent, emp::Random & rnd) { return this->Mutate(agent, rnd); });
    world->SetFitFun([this](Agent & agent) { return this->CalcFitness__MANCALA(agent); });

    // Configure instruction/event libraries.
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();

    // - Setup the instruction set -
    // Standard (SGP) instructions:
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

    // Configure problem-specific instructions
    if (PROBLEM == PROBLEM_ID__MANCALA) {
      // Setup game evaluation function.
      eval_game = [this](hardware_t & hw0, hardware_t & hw1) {
        return this->mancala.EvalMancala(hw0, hw1);
      };

      mancala.SetMoveEvalTime(EVAL_TIME);

      // How do we extract a move?
      mancala.get_move = [this](const hardware_t & hw) { return hw.GetTrait(TRAIT_ID__MOVE); };

      // Mancala-specific instructions.
      inst_lib->AddInst("IsValid", [this](hardware_t & hw, const inst_t & inst) {
        state_t & state = hw.GetCurState();
        const int val = this->mancala.GetCurGame().IsMoveValid(state.GetLocal(inst.args[0]));
        state.SetLocal(inst.args[1], val);
      }, 2, "WM[ARG2]=IsValidMove(WM[ARG1])");
      inst_lib->AddInst("EndTurn", [this](hardware_t & hw, const inst_t & inst) {
        hw.SetTrait(TRAIT_ID__DONE, 1);
      }, 0, "End turn.");

      if (MANCALA__GET_MOVE_METHOD == MANCALA__GET_MOVE_METHOD_ID__WM) {
        inst_lib->AddInst("SetMove", [this](hardware_t & hw, const inst_t & inst) {
          state_t & state = hw.GetCurState();
          hw.SetTrait(TRAIT_ID__MOVE, ((int)state.GetLocal(inst.args[0])) % 5);
        }, 1, "SetMove(WM[ARG1])");
      } else if (MANCALA__GET_MOVE_METHOD == MANCALA__GET_MOVE_METHOD_ID__OM) {
        inst_lib->AddInst("SetMove", [this](hardware_t & hw, const inst_t & inst) {
          state_t & state = hw.GetCurState();
          int move = 0;
          for (int i = 1; i < 6; ++i) {
            if (state.GetOutput(move) < state.GetOutput(i)) { move = i; }
          }
          hw.SetTrait(TRAIT_ID__MOVE, move);
        }, 1, "SetMove(WM[ARG1])");
      } else {
        std::cout << "Unrecognized Mancala get move method! Exiting..." << std::endl;
        exit(-1);
      }

      if (!RESET_HW_BETWEEN_MOVES) {
        mancala.reset_hw = [this](hardware_t & hw, const memory_t & main_in_mem=memory_t()) {
          hw.SetTrait(TRAIT_ID__DONE, 0);
        };
        inst_lib->AddInst("SenseBoard", [this](hardware_t & hw, const inst_t & inst) {
          state_t & state = hw.GetCurState();
          auto board = this->mancala.GetCurGame().AsInput(hw.GetTrait(TRAIT_ID__PLAYER_ID));
          for (auto mem : board) { state.SetInput(mem.first, mem.second); }
        }, 0, "Load board into input buffer.");
      } else {
        mancala.reset_hw = [this](hardware_t & hw, const memory_t & main_in_mem=memory_t()) {
          this->ResetHW(hw, main_in_mem);
          hw.SetTrait(TRAIT_ID__PLAYER_ID, this->mancala.GetCurGame().GetCurPlayer());
        };
      }

    } else if (PROBLEM == PROBLEM_ID__OTHELLO) {

    } else {
      std::cout << "Unrecognized PROBLEM. Exiting..." << std::endl;
      exit(-1);
    }

    // Configure evaluation hardware.
    eval_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);
    eval_hw->SetMinBindThresh(HW_MIN_BIND_THRESH);
    eval_hw->SetMaxCores(HW_MAX_CORES);
    eval_hw->SetMaxCallDepth(HW_MAX_CALL_DEPTH);
    opp_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);
    opp_hw->SetMinBindThresh(HW_MIN_BIND_THRESH);
    opp_hw->SetMaxCores(HW_MAX_CORES);
    opp_hw->SetMaxCallDepth(HW_MAX_CALL_DEPTH);

    // Setup the data/systematics output files.
    if (!ANALYZE_MODE) {
      auto & sys_file = world->SetupSystematicsFile(DATA_DIRECTORY + "systematics.csv");
      sys_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
      auto & fit_file = world->SetupFitnessFile(DATA_DIRECTORY + "fitness.csv");
      fit_file.SetTimingRepeat(FITNESS_INTERVAL);
    }

  }

  ~GamesExp() {
    eval_hw.Delete();
    opp_hw.Delete();
    world.Delete();
    inst_lib.Delete();
    event_lib.Delete();
    random.Delete();
  }

  // Run the experiment (in some mode or another)!
  void Run() {
    if (ANALYZE_MODE) {
      RunAnalysis();
    } else {
      RunExperiment();
    }
  }

  void ResetEvalHW() { ResetHW(*eval_hw); }

  void ResetOppHW() { ResetHW(*opp_hw); }

  void ResetHW(hardware_t & hw, const memory_t & main_in_mem=memory_t()) {
    hw.ResetHardware();
    hw.SetTrait(TRAIT_ID__MOVE, -1);
    hw.SetTrait(TRAIT_ID__DONE, 0);
    hw.SetTrait(TRAIT_ID__PLAYER_ID, -1);
    hw.SpawnCore(0, main_in_mem, true);
  }

  void LoadEvalHWProgram(const program_t & program) { LoadHWProgram(program, *eval_hw); }

  void LoadOppHWProgram(const program_t & program) { LoadHWProgram(program, *opp_hw); }

  void LoadHWProgram(const program_t & program, hardware_t & hw) {
    hw.SetProgram(program);
    ResetHW(hw);
  }

  /// Run in experiment mode. Either load individual ancestor or an entire population.
  /// After initializing the population, run evolution for the specified number of generations.
  void RunExperiment() {
    std::cout << "\nRunning experiment...\n" << std::endl;
    if (RUN_FROM_EXISTING_POP) {
      std::cout << "Too bad... not implemented yet..." << std::endl;
    } else {
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
      world->Inject(ancestor_prog, POP_SIZE);    // Inject a bunch of ancestors into the population.
    }

    // Population initialization done, ready to run evolution!
    double best_score = -32767;
    for (size_t ud = 0; ud <= GENERATIONS; ++ud) {
      best_score = -32767;
      // Evaluate each agent.
      for (size_t id = 0; id < world->GetSize(); ++id) {
        // std::cout << " Evaluating " << id << std::endl;
        // Our hero.
        Agent & our_hero = world->GetOrg(id);
        LoadEvalHWProgram(our_hero.GetGenome());
        // The evil antagonist.
        Agent & opponent = world->GetRandomOrg();
        LoadOppHWProgram(opponent.GetGenome());
        // Who makes the first move?
        mancala.SetStartPlayer((size_t)random->P(0.5));
        // Evaluate game.
        our_hero.score = eval_game(*eval_hw, *opp_hw);
        double fitness = world->CalcFitnessOrg(our_hero);
        // std::cout << "  Score: " << fitness << std::endl;
        if (fitness > best_score) { best_score = fitness; }
      }
      // Selection!
      emp::EliteSelect(*world, 1, 1);
      emp::TournamentSelect(*world, 4, POP_SIZE - 1);
      // Print out in-run summary stats.
      std::cout << "Update: " << world->GetUpdate() << "  Max score: " << best_score << std::endl;

      // Update the world.
      world->Update();

      // Mutate everyone.
      world->DoMutations(1);

      // Population snapshot?
      if (ud % POP_SNAPSHOT_INTERVAL == 0) SnapshotSF(ud);
    }

  }

  void RunAnalysis() {
    std::cout << "\nRunning analysis...\n" << std::endl;
  }

  /// Mutate organism function.
  /// Return number of mutation *events* that occur (e.g. function duplication, slip mutation are single events).
  size_t Mutate(Agent & agent, emp::Random & rnd) {
    program_t & program = agent.program;
    size_t mut_cnt = 0;
    // Duplicate a function?
    if (rnd.P(PER_FUNC__FUNC_DUP_RATE) && program.GetSize() < PROG_MAX_FUNC_CNT) {
      ++mut_cnt;
      const uint32_t fID = rnd.GetUInt(program.GetSize());
      program.PushFunction(program[fID]);
    }
    // Delete a function?
    if (rnd.P(PER_FUNC__FUNC_DEL_RATE) && program.GetSize() > 1) {
      ++mut_cnt;
      const uint32_t fID = rnd.GetUInt(program.GetSize());
      program[fID] = program[program.GetSize() - 1];
      program.program.resize(program.GetSize() - 1);
    }
    // For each function...
    for (size_t fID = 0; fID < program.GetSize(); ++fID) {
      // Mutate affinity
      for (size_t i = 0; i < program[fID].GetAffinity().GetSize(); ++i) {
        tag_t & aff = program[fID].GetAffinity();
        if (rnd.P(PER_BIT__TAG_BFLIP_RATE)) {
          ++mut_cnt;
          aff.Set(i, !aff.Get(i));
        }
      }
      // Slip-mutation?
      if (rnd.P(PER_FUNC__SLIP_RATE)) {
        uint32_t begin = rnd.GetUInt(program[fID].GetSize());
        uint32_t end = rnd.GetUInt(program[fID].GetSize());
        if (begin < end && ((program[fID].GetSize() + (end - begin)) < PROG_MAX_FUNC_LEN)) {
          // duplicate begin:end
          ++mut_cnt;
          const size_t dup_size = end - begin;
          const size_t new_size = program[fID].GetSize() + dup_size;
          hardware_t::Function new_fun(program[fID].GetAffinity());
          for (size_t i = 0; i < new_size; ++i) {
            if (i < end) new_fun.PushInst(program[fID][i]);
            else new_fun.PushInst(program[fID][i - dup_size]);
          }
          program[fID] = new_fun;
        } else if (begin > end && ((program[fID].GetSize() - (begin - end)) >= 1)) {
          // delete end:begin
          ++mut_cnt;
          hardware_t::Function new_fun(program[fID].GetAffinity());
          for (size_t i = 0; i < end; ++i)
            new_fun.PushInst(program[fID][i]);
          for (size_t i = begin; i < program[fID].GetSize(); ++i)
            new_fun.PushInst(program[fID][i]);
          program[fID] = new_fun;
        }
      }
      // Substitution mutations?
      for (size_t i = 0; i < program[fID].GetSize(); ++i) {
        inst_t & inst = program[fID][i];
        // Mutate affinity (even if it doesn't have one).
        for (size_t k = 0; k < inst.affinity.GetSize(); ++k) {
          if (rnd.P(PER_BIT__TAG_BFLIP_RATE)) {
            ++mut_cnt;
            inst.affinity.Set(k, !inst.affinity.Get(k));
          }
        }
        // Mutate instruction.
        if (rnd.P(PER_INST__SUB_RATE)) {
          ++mut_cnt;
          inst.id = rnd.GetUInt(program.GetInstLib()->GetSize());
        }
        // Mutate arguments (even if they aren't relevent to instruction).
        for (size_t k = 0; k < hardware_t::MAX_INST_ARGS; ++k) {
          if (rnd.P(PER_INST__SUB_RATE)) {
            ++mut_cnt;
            inst.args[k] = rnd.GetInt(PROG_MAX_ARG_VAL);
          }
        }
      }
      // Insertion/deletion mutations?
      // - Compute insertions.
      int num_ins = rnd.GetRandBinomial(program[fID].GetSize(), PER_INST__INS_RATE);
      // Ensure that insertions (+deletions) don't exceed maximum function length.
      if ((num_ins + program[fID].GetSize()) > PROG_MAX_FUNC_LEN) {
        num_ins = PROG_MAX_FUNC_LEN - program[fID].GetSize();
      }
      // Do we need to do any insertions or deletions?
      if (num_ins > 0 || PER_INST__DEL_RATE > 0.0) {
        emp::vector<size_t> ins_locs = emp::RandomUIntVector(rnd, num_ins, 0, program[fID].GetSize());
        if (ins_locs.size()) std::sort(ins_locs.begin(), ins_locs.end(), std::greater<size_t>());
        hardware_t::Function new_fun(program[fID].GetAffinity());
        size_t rhead = 0;
        size_t num_dels = 0;
        while (rhead < program[fID].GetSize()) {
          if (ins_locs.size()) {
            if (rhead >= ins_locs.back()) {
              // Insert a random instruction.
              new_fun.PushInst(rnd.GetUInt(program.GetInstLib()->GetSize()),
                               rnd.GetInt(PROG_MAX_ARG_VAL),
                               rnd.GetInt(PROG_MAX_ARG_VAL),
                               rnd.GetInt(PROG_MAX_ARG_VAL),
                               tag_t());
              new_fun.inst_seq.back().affinity.Randomize(rnd);
              ++mut_cnt;
              ins_locs.pop_back();
              continue;
            }
          }
          // Do we delete this instruction?
          if (rnd.P(PER_INST__DEL_RATE) && num_dels < (program[fID].GetSize() - 1)) {
            ++mut_cnt;
            ++num_dels;
          } else {
            new_fun.PushInst(program[fID][rhead]);
          }
          ++rhead;
        }
        program[fID] = new_fun;
      }
    }
    return mut_cnt;
  }

  double CalcFitness__MANCALA(Agent & agent) {
    return agent.score;
  }

  /// This function takes a snapshot of the world.
  /// Dump into single file (necessary instead of Snapshot to run on scratch drive on hpcc)
  void SnapshotSF(size_t update) {
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

  /// Instruction: Fork
  /// Description: Fork thread with local memory as new thread's input buffer.
  static void Inst_Fork(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    hw.SpawnCore(inst.affinity, hw.GetMinBindThresh(), state.local_mem);
  }


};


int main(int argc, char * argv[]) {
  // Read configs.
  std::string config_fname = "configs.cfg";
  auto args = emp::cl::ArgManager(argc, argv);
  GamesConfig config;
  config.Read(config_fname);
  if (args.ProcessConfigOptions(config, std::cout, config_fname, "games-config.h") == false) exit(0);
  if (args.TestUnknown() == false) exit(0);

  std::cout << "==============================" << std::endl;
  std::cout << "|    How am I configured?    |" << std::endl;
  std::cout << "==============================" << std::endl;
  config.Write(std::cout);
  std::cout << "==============================\n" << std::endl;

  GamesExp e(config);
  e.Run();

}
