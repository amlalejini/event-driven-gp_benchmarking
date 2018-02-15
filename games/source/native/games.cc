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
#include <cmath>

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
#include "games/Othello.h"
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
//  [ ] Systematically test stuff.

constexpr size_t TAG_WIDTH = 16;

constexpr size_t TRAIT_ID__MOVE = 0;
constexpr size_t TRAIT_ID__DONE = 1;
constexpr size_t TRAIT_ID__PLAYER_ID = 2;

// Problem IDs
constexpr size_t PROBLEM_ID__MANCALA = 0;
constexpr size_t PROBLEM_ID__OTHELLO = 1;

// Fitness methods IDs.
constexpr size_t FITNESS_CALC_ID__MIN = 0;
constexpr size_t FITNESS_CALC_ID__MAX = 1;
constexpr size_t FITNESS_CALC_ID__MEDIAN = 2;
constexpr size_t FITNESS_CALC_ID__AVG = 3;

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
    emp::vector<double> scores_by_trial;

    Agent(const program_t & _p)
      : program(_p), scores_by_trial()
    { ; }

    Agent(const Agent && in)
      : program(in.program), scores_by_trial(in.scores_by_trial)
    { ; }

    Agent(const Agent & in)
      : program(in.program), scores_by_trial(in.scores_by_trial)
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

  // Othello parameters.
  size_t OTHELLO__BOARD_SIZE;

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

  // TODO: edits to mancala to better support other player types.
  struct MancalaGame {
    using mancala_ai_t = std::function<size_t(emp::Mancala & game, bool)>;
    emp::Mancala mancala_game;

    std::function<size_t(const hardware_t &)> get_move;
    std::function<bool(const hardware_t &)> get_done;
    std::function<void(hardware_t &, const memory_t &)> begin_turn;
    std::function<void(hardware_t &, size_t)> begin_game;

    size_t move_eval_time;
    bool verbose;
    size_t start_player;

    struct GameStats {
      size_t rounds;
      double p0_score; ///< Our hero's score.
      double p1_score; ///< Opponent score.
      size_t p0_errors;
      size_t p1_errors;

      GameStats()
        : rounds(0), p0_score(0.0), p1_score(0.0), p0_errors(0), p1_errors(0)
      { ; }

      void Reset() {
        rounds = 0; p0_score = 0; p1_score = 0;
        p0_errors = 0; p1_errors = 0;
      }
    } stats;


    MancalaGame()
      : mancala_game(), get_move(), get_done(), begin_turn(), begin_game(),
        move_eval_time(512), verbose(false), start_player(0), stats()
    { ; }

    emp::Mancala & GetCurGame() { return mancala_game; }

    void SetVerbose(bool v) { verbose = v; }
    void SetStartPlayer(size_t p) { start_player = p; }

    void SetMoveEvalTime(size_t t) { move_eval_time = t; }

    /// Based on Mancala example in Empirical library (originally written by Charles Ofria).
    double EvalMancala(mancala_ai_t & player0, mancala_ai_t & player1) {
      size_t cur_player = start_player;
      mancala_game.Reset(cur_player==0);
      stats.Reset();
      while (mancala_game.IsDone() == false) {
        // Determine the current player and their move.
        auto & play_fun = (cur_player == 0) ? player0 : player1;
        bool promise_validity = (cur_player == 0) ? false : true;
        size_t best_move = play_fun(mancala_game, promise_validity);

        if (verbose) {
          std::cout << "round = " << stats.rounds << "   errors = " << stats.p0_errors << std::endl;
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
          if (cur_player == 0) {
            stats.p0_errors++;
          } else {
            stats.p1_errors++;
          }
          if (++best_move > 5) best_move = 0;
        }

        // Do the move and determine who goes next.
        bool go_again = mancala_game.DoMove(cur_player, best_move);
        if (!go_again) cur_player = !cur_player;
        stats.rounds++;
      }

      if (verbose) {
        std::cout << "Final scores -- A: " << mancala_game.ScoreA()
                  << "   B: " << mancala_game.ScoreB()
        << std::endl;
      }

      stats.p0_score = mancala_game.ScoreA();
      stats.p1_score = mancala_game.ScoreB();

      return ((double)mancala_game.ScoreA()) - ((double)mancala_game.ScoreB());
    }

    /// Based on Mancala example in Empirical library (originally written by Charles Ofria)
    double EvalMancala(hardware_t & hw0, hardware_t & hw1) {
      begin_game(hw0, 0);
      begin_game(hw1, 1);
      mancala_ai_t agent0_fun = [&hw0, this](emp::Mancala & game, bool promise_validity=false){ return this->EvalMancalaMove(game, hw0, promise_validity); };
      mancala_ai_t agent1_fun = [&hw1, this](emp::Mancala & game, bool promise_validity=false){ return this->EvalMancalaMove(game, hw1, promise_validity); };
      return EvalMancala(agent0_fun, agent1_fun);
    }

    size_t EvalMancalaMove(emp::Mancala & game, hardware_t & hw, bool promise_validity=false) {
      // Reset hardware.
      begin_turn(hw, game.AsInput(game.GetCurPlayer()));
      // Run code until time runs out or until move has been set.
      for (size_t i = 0; i < move_eval_time && get_done(hw); ++i) {
        hw.SingleProcess();
      }
      size_t move = get_move(hw);

      // Did we promise to give a valid move?
      if (promise_validity) {
        // If the chosen move is illegal, shift through other options.
        while (!mancala_game.IsMoveValid(move)) {
          if (++move > 5) move = 0;
        }
      }
      return move;
    }
  };

  struct OthelloGame {
    using othello_ai_t = std::function<size_t(emp::Othello & game, bool)>;
    using space_t = emp::Othello::BoardSpace;
    emp::Othello othello_game;
    std::function<size_t(const hardware_t &)> get_move;
    std::function<bool(const hardware_t &)> get_done;
    std::function<void(hardware_t &)> begin_turn;
    std::function<void(hardware_t &, size_t)> begin_game;

    size_t move_eval_time;
    bool verbose;

    struct GameStats {
      size_t rounds;
      double p0_score;
      double p1_score;
      bool finished_game;
      size_t focal_playerID;

      GameStats()
        : rounds(0), p0_score(0.0), p1_score(0.0), finished_game(false), focal_playerID(0)
      { ; }

      void Reset() {
        rounds = 0; p0_score = 0; p1_score = 0; finished_game = false;
        focal_playerID = 0;
      }
    } stats;

    OthelloGame(size_t _board_size)
      : othello_game(_board_size),
        get_move(), get_done(), begin_turn(), begin_game(),
        move_eval_time(512), verbose(false), stats()
    { ; }

    emp::Othello & GetCurGame() { return othello_game; }

    void SetMoveEvalTime(size_t t) { move_eval_time = t; }

    void SetVerbose(bool v) { verbose = v; }

    // double EvalMancala(mancala_ai_t & player0, mancala_ai_t & player1) {
    double EvalOthello(othello_ai_t & player0, othello_ai_t & player1, bool focal_player0=true) {
      othello_game.Reset();
      stats.Reset();
      stats.focal_playerID = (focal_player0) ? 0 : 1;
      size_t player = 0;
      while (!othello_game.IsOver()) {
        player = othello_game.GetCurPlayer();
        // TODO: check this promise validity...
        bool promise_validity = (player == 0) ? !focal_player0 : focal_player0;
        // If player0 && focal_player0: no promise.
        // If player1 && !focal_player0: no promise.
        // If player0 && !focal_player0: promise.
        // If player1 && focal_player0: promise.
        auto & play_fun = (player == 0) ? player0 : player1;
        size_t best_move = play_fun(othello_game, promise_validity);

        if (verbose) {
          std::cout << "--- BOARD STATE ---" << std::endl;
          othello_game.Print();
          std::cout << "Player: " << player << std::endl;
          std::cout << "Round: " << stats.rounds << std::endl;
          std::cout << "Move: " << othello_game.GetPosX(best_move) << ", " << othello_game.GetPosY(best_move) << std::endl;
          if (!othello_game.IsMoveValid(player, best_move)) { std::cout << "Illegal move!" << std::endl; }
        }

        // If we're not promising validity
        //  - End the game on illegal move.
        if (!promise_validity) {
          if (!othello_game.IsMoveValid(player, best_move)) {
            break;
          }
        }

        // All is swell, go ahead with move.
        othello_game.DoMove(player, best_move);
        stats.rounds++;
      }

      // Fill out stats.
      stats.finished_game = othello_game.IsOver();
      stats.p0_score = othello_game.GetScore(0);
      stats.p1_score = othello_game.GetScore(1);

      return (focal_player0) ? stats.p0_score : stats.p1_score;
    }

    // double EvalMancala(hardware_t & hw0, hardware_t & hw1) {
    double EvalOthello(hardware_t & hw0, hardware_t & hw1, bool focal_player0=true) {
      // DarkPlayer is always player0
      begin_game(hw0, 0);
      begin_game(hw1, 1);
      othello_ai_t agent0_fun = [&hw0, this](emp::Othello & game, bool promise_validity) { return EvalOthelloMove(game, hw0, promise_validity); };
      othello_ai_t agent1_fun = [&hw1, this](emp::Othello & game, bool promise_validity) { return EvalOthelloMove(game, hw1, promise_validity); };
      return EvalOthello(agent0_fun, agent1_fun, focal_player0);
    }

    // TODO: add more EvalOthello interface options --> more AI types.. a few heuristics... human player... etc..

    // Eval move on given hardware and on given game board state.
    size_t EvalOthelloMove(emp::Othello & game, hardware_t & hw, bool promise_validity=false) {
      // -- Steven's EvalMove: --
      // 1) Reset CPU hardware
      // 2) Load board into agent's memory.
      // 3) Run agent for EVAL_TIME
      // 4) Determine chosen move.
      // 5) return chosen move.
      // ----
      // Reset hardware as appropriate.
      begin_turn(hw);
      // Run code until time runs out or until move has been set.
      for (size_t i = 0; i < move_eval_time && get_done(hw); ++i) {
        hw.SingleProcess();
      }
      size_t move = get_move(hw);

      // If this agent ai is promising validity, we need to make that happen....
      if (promise_validity) {
        // Double-check that move is valid.
        const size_t playerID = hw.GetTrait(TRAIT_ID__PLAYER_ID);
        if (!othello_game.IsMoveValid(playerID, move)) {
          // Move isn't valid... Need to make it valid.
          // Find nearest valid move.
          emp::vector<size_t> valid_moves = othello_game.GetMoveOptions(playerID);
          const size_t move_x = othello_game.GetPosX(move);
          const size_t move_y = othello_game.GetPosY(move);
          size_t new_move_x = 0;
          size_t new_move_y = 0;
          size_t sq_move_dist = othello_game.GetBoard().size() * othello_game.GetBoard().size();
          for (size_t i = 0; i < valid_moves.size(); ++i) {
            const size_t proposed_x = othello_game.GetPosX(valid_moves[i]);
            const size_t proposed_y = othello_game.GetPosY(valid_moves[i]);
            const size_t proposed_dist = std::pow(proposed_x - move_x, 2) + std::pow(proposed_y - move_y, 2);
            if (proposed_dist < sq_move_dist) {
              new_move_x = proposed_x; new_move_y = proposed_y; sq_move_dist = proposed_dist;
            }
          }
          move = othello_game.GetPosID(new_move_x, new_move_y);
        }
      }
      return move;
    }

    // Agent-board interaction wrappers.
    int GetBoardValue(size_t playerID, size_t x, size_t y) {
      return GetBoardValue(playerID, othello_game.GetPosID(x, y));
    }

    int GetBoardValue(size_t playerID, size_t posID) {
      int owner = othello_game.GetPosOwner(posID);
      if (owner == playerID) return 1;
      else if (owner == othello_game.GetOpponentID(playerID)) return -1;
      else return 0;
    }

  };

  std::function<double(hardware_t &, hardware_t &)> eval_game;
  std::function<double(void)> calc_score; ///< Calculate eval agent's score based on CURRENT game state.

  MancalaGame mancala;
  OthelloGame othello;


public:
  GamesExp(const GamesConfig & config)
    : eval_game(), calc_score(), mancala(), othello(4)
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
    OTHELLO__BOARD_SIZE = config.OTHELLO__BOARD_SIZE();
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

    if (FITNESS_CALC_TYPE == FITNESS_CALC_ID__MIN) {
      world->SetFitFun([this](Agent & agent) { return this->CalcFitness__MIN(agent); });
    } else if (FITNESS_CALC_TYPE == FITNESS_CALC_ID__MAX) {
      world->SetFitFun([this](Agent & agent) { return this->CalcFitness__MAX(agent); });
    } else if (FITNESS_CALC_TYPE == FITNESS_CALC_ID__MEDIAN) {
      world->SetFitFun([this](Agent & agent) { return this->CalcFitness__MEDIAN(agent); });
    } else if (FITNESS_CALC_TYPE == FITNESS_CALC_ID__AVG) {
      world->SetFitFun([this](Agent & agent) { return this->CalcFitness__AVG(agent); });
    } else {
      std::cout << "Unrecognized fitness calculation type. Exiting..." << std::endl;
      exit(-1);
    }


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
      // How much time (max) do we give for evaluations?
      mancala.SetMoveEvalTime(EVAL_TIME);

      // TODO: setup a variety of calc_score functions options.
      calc_score = [this]() {
        return this->mancala.stats.p0_score - this->mancala.stats.p1_score - ((double)this->mancala.stats.p0_errors*10);
      };

      // Setup game evaluation function.
      eval_game = [this](hardware_t & hw0, hardware_t & hw1) {
        mancala.SetStartPlayer((size_t)this->random->P(0.5));
        this->mancala.EvalMancala(hw0, hw1);
        return calc_score();
      };

      // How do we extract a move?
      mancala.get_move = [this](const hardware_t & hw) { return hw.GetTrait(TRAIT_ID__MOVE); };

      mancala.get_done = [this](const hardware_t & hw) { return (bool)hw.GetTrait(TRAIT_ID__DONE); };

      mancala.begin_game = [this](hardware_t & hw, size_t playerID) {
        this->ResetHW(hw);
        hw.SetTrait(TRAIT_ID__PLAYER_ID, playerID);
      };

      // Mancala-specific instructions.

      // Signal that agent is done with its turn.
      inst_lib->AddInst("EndTurn", [this](hardware_t & hw, const inst_t & inst) {
        hw.SetTrait(TRAIT_ID__DONE, 1);
      }, 0, "End turn.");

      if (MANCALA__GET_MOVE_METHOD == MANCALA__GET_MOVE_METHOD_ID__WM) {
        // Set move from working memory location specified by instruction argument.
        inst_lib->AddInst("SetMove", [this](hardware_t & hw, const inst_t & inst) {
          state_t & state = hw.GetCurState();
          hw.SetTrait(TRAIT_ID__MOVE, ((int)state.GetLocal(inst.args[0])) % 5);
        }, 1, "SetMove(WM[ARG1])");
        // Is the proposed move (WM[ARG1]) valid?
        inst_lib->AddInst("IsValid", [this](hardware_t & hw, const inst_t & inst) {
            state_t & state = hw.GetCurState();
            const int val = this->mancala.GetCurGame().IsMoveValid(state.GetLocal(inst.args[0]));
            state.SetLocal(inst.args[1], val);
          }, 2, "WM[ARG2]=IsValidMove(WM[ARG1])");
      } else if (MANCALA__GET_MOVE_METHOD == MANCALA__GET_MOVE_METHOD_ID__OM) {
        // Set move using output memory of active function.
        inst_lib->AddInst("SetMove", [this](hardware_t & hw, const inst_t & inst) {
            state_t & state = hw.GetCurState();
            int move = 0;
            for (int i = 1; i < 6; ++i) {
              if (state.GetOutput(move) < state.GetOutput(i)) { move = i; }
            }
            hw.SetTrait(TRAIT_ID__MOVE, move);
          }, 1, "SetMove(WM[ARG1])");

        // Is the proposed move (WM[ARG1]) valid?
        inst_lib->AddInst("IsValid", [this](hardware_t & hw, const inst_t & inst) {
            state_t & state = hw.GetCurState();
            // figure out move.
            int move = 0;
            for (int i = 1; i < 6; ++i) {
              if (state.GetOutput(move) < state.GetOutput(i)) { move = i; }
            }
            const int val = this->mancala.GetCurGame().IsMoveValid(move);
            state.SetLocal(inst.args[1], val);
          }, 2, "WM[ARG2]=IsValidMove(WM[ARG1])");

      } else {
        std::cout << "Unrecognized Mancala get move method! Exiting..." << std::endl;
        exit(-1);
      }

      if (!RESET_HW_BETWEEN_MOVES) {
        mancala.begin_turn = [this](hardware_t & hw, const memory_t & main_in_mem=memory_t()) {
          hw.SetTrait(TRAIT_ID__DONE, 0);
        };
        inst_lib->AddInst("SenseBoard", [this](hardware_t & hw, const inst_t & inst) {
          state_t & state = hw.GetCurState();
          auto board = this->mancala.GetCurGame().AsInput(hw.GetTrait(TRAIT_ID__PLAYER_ID));
          for (auto mem : board) { state.SetInput(mem.first, mem.second); }
        }, 0, "Load board into input buffer.");
      } else {
        mancala.begin_turn = [this](hardware_t & hw, const memory_t & main_in_mem=memory_t()) {
          this->ResetHW(hw, main_in_mem);
          hw.SetTrait(TRAIT_ID__PLAYER_ID, this->mancala.GetCurGame().GetCurPlayer());
        };
      }

    } else if (PROBLEM == PROBLEM_ID__OTHELLO) {
      othello = OthelloGame(OTHELLO__BOARD_SIZE);
      othello.SetMoveEvalTime(EVAL_TIME);

      // TODO: scoring functions.
      calc_score = [this](){
        // 1) #rounds w/out a mistake
        // 2) bonus for finishing game
        // 3) bonus for winning.
        double score = this->othello.stats.rounds;
        // If they finished the game...
        if (this->othello.stats.finished_game) {
          size_t max_rounds = this->othello.GetCurGame().GetBoard().size();
          score = max_rounds;
          double hero_score = 0;
          double opp_score = 0;
          if (this->othello.stats.focal_playerID == 0) {
            hero_score = this->othello.stats.p0_score;
            opp_score = this->othello.stats.p1_score;
          } else {
            hero_score = this->othello.stats.p1_score;
            opp_score = this->othello.stats.p0_score;
          }
          // Get bonus for winning.
          score += (hero_score - opp_score);
          // Get bonus for how quickly you finished the game if you won.
          if (hero_score > opp_score) {
            score += max_rounds - this->othello.stats.rounds;
          }
        }
        return score;
      };

      eval_game = [this](hardware_t & hw0, hardware_t & hw1) {
        if ((size_t)this->random->P(0.5)) {
          this->othello.EvalOthello(hw0, hw1, true);
        } else {
          this->othello.EvalOthello(hw1, hw0, false);
        }
        return this->calc_score();
      };
      // Othello game interface requires:
      //  - get_move
      othello.get_move = [this](const hardware_t & hw) { return hw.GetTrait(TRAIT_ID__MOVE); };
      //  - get_done
      othello.get_done = [this](const hardware_t & hw) { return (bool)hw.GetTrait(TRAIT_ID__DONE); };
      //  - begin_turn
      // Currently only supporting reset between turns...
      othello.begin_turn = [this](hardware_t & hw) {
        this->ResetHW(hw);
        hw.SetTrait(TRAIT_ID__PLAYER_ID, this->othello.GetCurGame().GetCurPlayer());
      };
      //  - begin_game
      othello.begin_game = [this](hardware_t & hw, size_t playerID) {
        this->ResetHW(hw);
        hw.SetTrait(TRAIT_ID__PLAYER_ID, playerID);
      };

      // TODO: configure othello-specific hardware.

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
        for (size_t trialID = 0; trialID < TRIAL_CNT; ++trialID) {
          // Evaluate game.
          our_hero.scores_by_trial.emplace_back(eval_game(*eval_hw, *opp_hw));
        }
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

  double CalcFitness__MIN(Agent & agent) {
    if (!agent.scores_by_trial.size()) return 0.0;
    double val = agent.scores_by_trial[0];
    for (size_t i = 0; i < agent.scores_by_trial.size(); ++i) {
      if (agent.scores_by_trial[i] < val) val = agent.scores_by_trial[i];
    }
    return val;
  }

  double CalcFitness__MEDIAN(Agent & agent) {
    if (!agent.scores_by_trial.size()) return 0.0;
    // Sort scores by trial.
    std::sort(agent.scores_by_trial.begin(), agent.scores_by_trial.end());
    return agent.scores_by_trial[(size_t)(agent.scores_by_trial.size() / 2)];
  }

  double CalcFitness__AVG(Agent & agent) {
    if (!agent.scores_by_trial.size()) return 0.0;
    double val = 0.0;
    for (size_t i = 0; i < agent.scores_by_trial.size(); ++i) {
      val += agent.scores_by_trial[i];
    }
    return val / agent.scores_by_trial.size();
  }

  double CalcFitness__MAX(Agent & agent) {
    if (!agent.scores_by_trial.size()) return 0.0;
    double val = agent.scores_by_trial[0];
    for (size_t i = 0; i < agent.scores_by_trial.size(); ++i) {
      if (agent.scores_by_trial[i] > val) val = agent.scores_by_trial[i];
    }
    return val;
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
