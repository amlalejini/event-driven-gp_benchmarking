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
#include "Evo/World.h"
#include "logic_operations-config.h"

// At the moment, only supports 16 bit tags.
constexpr size_t TAG_WIDTH_8  = 8;
constexpr size_t TAG_WIDTH_16 = 16;
constexpr size_t TAG_WIDTH_32 = 32;

constexpr size_t TASK_INPUT_CNT = 2;

constexpr size_t TASK_CNT = 10;
constexpr size_t TASK_ID__NAND = 0;
constexpr size_t TASK_ID__NOT = 1;
constexpr size_t TASK_ID__ORN = 2;
constexpr size_t TASK_ID__AND = 3;
constexpr size_t TASK_ID__OR = 4;
constexpr size_t TASK_ID__ANDN = 5;
constexpr size_t TASK_ID__NOR = 6;
constexpr size_t TASK_ID__XOR = 7;
constexpr size_t TASK_ID__EQU = 8;
constexpr size_t TASK_ID__ECHO = 9;

using task_input_t = uint32_t;
constexpr task_input_t MIN_TASK_INPUT = 0; //-32767;
constexpr task_input_t MAX_TASK_INPUT = 1000000000; //32767;

constexpr size_t PROBLEM_ID__TASKS = 0;
constexpr size_t PROBLEM_ID__CHANGING_ENV = 1;
constexpr size_t PROBLEM_ID__CHANGING_ENV_WITH_TASKS = 2;

constexpr size_t FIT_TYPE__MIN = 0;
constexpr size_t FIT_TYPE__AVG = 1;

// Hardware trait indices.
constexpr size_t TRAIT_ID__STATE = 0; ///< Agent internal state trait.

// Available environment affinity strings.
emp::vector<std::string> env_hc_tag_16_strs = {"0000000000000000",
                                               "1111111111111111",
                                               "1111000000001111",
                                               "0000111111110000",
                                               "1111000011110000",
                                               "0000111100001111",
                                               "0000000011111111",
                                               "1111111100000000",
                                               "0110011001100110",
                                               "1001100110011001",
                                               "1001011001101001",
                                               "0110100110010110",
                                               "0110011010011001",
                                               "1001100101100110",
                                               "1001011010010110",
                                               "0110100101101001",
                                               "0011001100110011",
                                               "1100110011001100",
                                               "1100001100111100",
                                               "0011110011000011",
                                               "1100001111000011",
                                               "0011110000111100",
                                               "0011001111001100",
                                               "1100110000110011"
                                               };


size_t MAX_ENV_HC_STATES = env_hc_tag_16_strs.size(); ///< Maximum possible number of environment states.

/// Class to manage a changing environment Signal GP benchmark experiment.
///  - Will be configured based on given configs (i.e. treatment parameters).
class LogicOperationsExp {
public:
  using hardware_t = emp::EventDrivenGP_AW<TAG_WIDTH_16>;
  using program_t = hardware_t::Program;
  using state_t = hardware_t::State;
  using inst_t = hardware_t::inst_t;
  using inst_lib_t = hardware_t::inst_lib_t;
  using event_t = hardware_t::event_t;
  using event_lib_t = hardware_t::event_lib_t;
  using memory_t = hardware_t::memory_t;
  using tag_t = hardware_t::affinity_t;

  /// Struct to keep track of agents.
  /// Agents are defined by a program and a score.
  struct Agent {
    program_t program;
    emp::vector<emp::vector<size_t>> task_completions_by_trial;
    emp::vector<emp::vector<size_t>> task_credits_by_trial;
    emp::vector<size_t> env_matches_by_trial;
    emp::vector<double> scores_by_trial;

    Agent(const program_t & _p) :
      program(_p),
      task_completions_by_trial(),
      task_credits_by_trial(),
      env_matches_by_trial(),
      scores_by_trial()
    { ; }

    Agent(Agent && in)
      : program(in.program),
        task_completions_by_trial(in.task_completions_by_trial),
        task_credits_by_trial(in.task_credits_by_trial),
        env_matches_by_trial(in.env_matches_by_trial),
        scores_by_trial(in.scores_by_trial)
    { ; }

    Agent(const Agent & in)
      : program(in.program),
        task_completions_by_trial(in.task_completions_by_trial),
        task_credits_by_trial(in.task_credits_by_trial),
        env_matches_by_trial(in.env_matches_by_trial),
        scores_by_trial(in.scores_by_trial)
    { ; }

    /// Required by Empirical's World.h.
    program_t & GetGenome() { return program; }
  };

  struct Task {
    std::string task;
    int id;
    emp::vector<task_input_t> solutions;
    size_t completed;
    size_t credited;
    emp::vector<size_t> comp_time_stamps;              // Time stamp for each time it was completed.
    emp::vector<size_t> cred_time_stamps;
    std::function<void(task_input_t,task_input_t)> calc_solutions;

    Task() : task(""), id(-1), solutions(), completed(0), credited(0), comp_time_stamps(), cred_time_stamps(), calc_solutions() { ; }

    Task(std::string _name, int _id, emp::vector<task_input_t> _sol = emp::vector<task_input_t>())
      : task(_name), id(_id), solutions(_sol), completed(0), credited(0), comp_time_stamps(), cred_time_stamps(), calc_solutions()
    { ; }
  };

  using world_t = emp::World<Agent>;

protected:
  // == Configurable experiment parameters. ==
  bool DEBUG_MODE;            ///< Are we in debug mode? NOTE: Currenly not in use.
  int RANDOM_SEED;            ///< Random seed to use for this experiment.
  size_t GENERATIONS;         ///< How generations should the experiment run for?
  size_t POP_SIZE;            ///< Population size.
  size_t EVAL_TIME;           ///< How long should we evaluate agents for?
  size_t TRIAL_CNT;           ///< How many trials should we evaluate each agent?
  std::string ANCESTOR_FPATH; ///< File path to ancestor program description.
  size_t PROBLEM;
  size_t FITNESS_CALC_TYPE;

  // Environment-specific settings.
  bool CHANGING_ENVIRONMENT;        ///< Is the environment changing?
  size_t ENVIRONMENT_STATES;        ///< Number of environment states.
  double ENVIRONMENT_CHG_PROB;      ///< Probability of environment change on update.
  bool RND_ENV_STATE_TAGS;

  // Hardware-specific settings.
  bool ENV_SIGNALS;          ///< Do environment changes trigger events?
  bool ACTIVE_SENSING;       ///< Is active sensing enabled?
  size_t HW_MAX_CORES;       ///< Max number of hardware cores. i.e. max number of simultaneous threads of execution hardware will support.
  size_t HW_MAX_CALL_DEPTH;  ///< Max call depth of hardware unit.
  double HW_MIN_BIND_THRESH; ///< Hardware minimum binding threshold.

  // Mutation-specific settings.
  size_t PROG_MAX_FUNC_CNT;       ///< Maximum function count per program?
  size_t PROG_MAX_FUNC_LEN;       ///< Maximum function length?
  size_t PROG_MAX_ARG_VAL;        ///< Maximum argument value that can mutate in.
  double PER_BIT__TAG_BFLIP_RATE; ///< Per-bit tag bit-flip rate?
  double PER_INST__SUB_RATE;      ///< Per-instruction substitution rate.
  double PER_INST__INS_RATE;      ///< Per-instruction insertion rate.
  double PER_INST__DEL_RATE;      ///< Per-instruction deletion rate.
  double PER_FUNC__SLIP_RATE;     ///< Per-function slip mutation rate.
  double PER_FUNC__FUNC_DUP_RATE; ///< Per-function duplication rate.
  double PER_FUNC__FUNC_DEL_RATE; ///< Per-function deletion rate.


  // Data output-specific settings.
  size_t SYSTEMATICS_INTERVAL;    ///< Interval to save summary statistics.
  size_t FITNESS_INTERVAL;        ///< Interval to save fitness statistics.
  size_t POPULATION_INTERVAL;     ///< Interval to save ?population? data?
  size_t POP_SNAPSHOT_INTERVAL;   ///< Interval to take full program snapshots of population.
  std::string DATA_DIRECTORY;     ///< Directory in which to store all program output.

  // Run from pop settings.
  bool RUN_FROM_EXISTING_POP;
  std::string EXISTING_POP_LOC;

  // Analysis settings.
  bool ANALYZE_MODE;               ///< Are we in analysis mode?
  size_t ANALYSIS;                 ///< Which analysis are we running?
  std::string ANALYZE_AGENT_FPATH; ///< Path to analysis agent program.
  size_t FDOM_ANALYSIS_TRIAL_CNT;  ///< How many times do we try each agent?
  bool TEASER_SENSORS;             ///< Deactivate sensors?
  bool TEASER_EVENTS;              ///< Disable env events?

  emp::Ptr<emp::Random> random;
  emp::Ptr<world_t> world;

  emp::Ptr<inst_lib_t> inst_lib;
  emp::Ptr<event_lib_t> event_lib;

  emp::Ptr<hardware_t> eval_hw;

  emp::vector<tag_t> env_state_tags; ///< Affinities associated with each environment state.

  emp::vector<Task> tasks_info;
  emp::vector<task_input_t> task_inputs;
  size_t load_id;

  int env_state;
  int eval_update;

  std::string analysis_scores_fname;

  std::function<double(Agent &, size_t)> score_agent;

public:
  LogicOperationsExp(const LogicOperationsConfig & config) : load_id(0), env_state(-1), eval_update(0) {
    // Fill out experiment parameters with config settings.
    DEBUG_MODE = config.DEBUG_MODE();
    RANDOM_SEED = config.RANDOM_SEED();
    GENERATIONS = config.GENERATIONS();
    POP_SIZE = config.POP_SIZE();
    EVAL_TIME = config.EVAL_TIME();
    TRIAL_CNT = config.TRIAL_CNT();
    FITNESS_CALC_TYPE = config.FITNESS_CALC_TYPE();
    ANCESTOR_FPATH = config.ANCESTOR_FPATH();
    PROBLEM = config.PROBLEM();
    ENVIRONMENT_STATES = config.ENVIRONMENT_STATES();
    ENVIRONMENT_CHG_PROB = config.ENVIRONMENT_CHG_PROB();
    RND_ENV_STATE_TAGS = config.RND_ENV_STATE_TAGS();
    ENV_SIGNALS = config.ENV_SIGNALS();
    ACTIVE_SENSING = config.ACTIVE_SENSING();
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
    FITNESS_INTERVAL = config.FITNESS_INTERVAL();
    POPULATION_INTERVAL = config.POPULATION_INTERVAL();
    POP_SNAPSHOT_INTERVAL = config.POP_SNAPSHOT_INTERVAL();
    DATA_DIRECTORY = config.DATA_DIRECTORY();
    RUN_FROM_EXISTING_POP = config.RUN_FROM_EXISTING_POP();
    EXISTING_POP_LOC = config.EXISTING_POP_LOC();
    ANALYZE_MODE = config.ANALYZE_MODE();
    ANALYSIS = config.ANALYSIS();
    ANALYZE_AGENT_FPATH = config.ANALYZE_AGENT_FPATH();
    FDOM_ANALYSIS_TRIAL_CNT = config.FDOM_ANALYSIS_TRIAL_CNT();
    TEASER_SENSORS = config.TEASER_SENSORS();
    TEASER_EVENTS = config.TEASER_EVENTS();

    // Setup the output directory.
    mkdir(DATA_DIRECTORY.c_str(), ACCESSPERMS);
    if (DATA_DIRECTORY.back() != '/') DATA_DIRECTORY += '/';

    // clean up existing population location directory.
    if (EXISTING_POP_LOC.back() != '/') EXISTING_POP_LOC += '/';

    // Make our random number generator.
    random = emp::NewPtr<emp::Random>(RANDOM_SEED);

    if (!ANALYZE_MODE) {
      // Configure the environment.
      if (RND_ENV_STATE_TAGS) {
        // Generate random (but unique) tags.
        std::unordered_set<int> uset;
        if (ENVIRONMENT_STATES > emp::Pow2(TAG_WIDTH_16)) {
          std::cout << "Requested environment states exceeds maximum environment states." << std::endl;
          std::cout << "Setting ENVIRONMENT_STATES to 2^TAG_WIDTH" << std::endl;
          ENVIRONMENT_STATES = emp::Pow2(TAG_WIDTH_16);
        }
        std::cout << "Randomly generating env tags: " << std::endl;
        for (size_t i = 0; i < ENVIRONMENT_STATES; ++i) {
          env_state_tags.emplace_back(tag_t());
          env_state_tags[i].Randomize(*random);
          int tag_int = env_state_tags[i].GetUInt(0);
          while (true) {
            if (!emp::Has(uset, tag_int)) {
              uset.emplace(tag_int);
              break;
            } else {
              env_state_tags[i].Randomize(*random);
              tag_int = env_state_tags[i].GetUInt(0);
            }
          }
        }
      } else {
        // Assign hand-coded tags to the environment.
        if (ENVIRONMENT_STATES > MAX_ENV_HC_STATES) {
          std::cout << "Requested environment states exceeds maximum environment states." << std::endl;
          std::cout << "Setting ENVIRONMENT_STATES to MAX_ENV_HC_STATES" << std::endl;
          ENVIRONMENT_STATES = MAX_ENV_HC_STATES;
        }
        for (size_t i = 0; i < ENVIRONMENT_STATES; ++i) {
          env_state_tags.emplace_back(tag_t());
          for (size_t c = 0; c < env_hc_tag_16_strs[i].size(); ++c) {
            if (env_hc_tag_16_strs[i][c] == '0') env_state_tags[i].Set(c, 0);
            else env_state_tags[i].Set(c, 1);
          }
        }
      }
      // Save out the environment states.
      std::ofstream envtags_ofstream(DATA_DIRECTORY + "env_tags.csv");
      envtags_ofstream << "env_id,tag\n";
      std::cout << "Environment states: " << std::endl;
      for (size_t i = 0; i < env_state_tags.size(); ++i) {
        int tag_int = env_state_tags[i].GetUInt(0);
        std::cout << "[" << i << "]: "; env_state_tags[i].Print(); std::cout << "(" << tag_int << ")" << std::endl;
        envtags_ofstream << i << ","; env_state_tags[i].Print(envtags_ofstream); envtags_ofstream << "\n";
      }
      envtags_ofstream.close();
    } else {
      // Load tags from local file.
      env_state_tags.resize(ENVIRONMENT_STATES, tag_t());

      std::ifstream tag_fstream("env_tags.csv");
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
        int state_id = std::stoi(line_components[0]);
        for (size_t i = 0; i < line_components[1].size(); ++i) {
          if (i >= TAG_WIDTH_16) break;
          if (line_components[1][i] == '1') env_state_tags[state_id].Set(env_state_tags[state_id].GetSize() - i - 1, true);
        }
      }
      tag_fstream.close();
    }

    // Print tags
    std::cout << "ENVIRONMENT STATE TAGS: " << std::endl;
    for (size_t i = 0; i < env_state_tags.size(); ++i) {
      std::cout << "State " << i << " tag: "; env_state_tags[i].Print(); std::cout << std::endl;
    }

    // Initialize task input buffer. (for evaluation hardware)
    task_inputs.resize(TASK_INPUT_CNT);
    // Initialize tasks_info
    tasks_info.resize(TASK_CNT);
    // NAND Task
    Task & nand_task = tasks_info[TASK_ID__NAND];
    nand_task.task = "NAND";
    nand_task.id = TASK_ID__NAND;
    nand_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__NAND].solutions.resize(0);
      this->tasks_info[TASK_ID__NAND].solutions.emplace_back(~(a&b));
    };
    // NOT Task
    Task & not_task = tasks_info[TASK_ID__NOT];
    not_task.task = "NOT";
    not_task.id = TASK_ID__NOT;
    not_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__NOT].solutions.resize(0);
      this->tasks_info[TASK_ID__NOT].solutions.emplace_back(~a);
      this->tasks_info[TASK_ID__NOT].solutions.emplace_back(~b);
    };
    // ORN Task
    Task & orn_task = tasks_info[TASK_ID__ORN];
    orn_task.task = "ORN";
    orn_task.id = TASK_ID__ORN;
    orn_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__ORN].solutions.resize(0);
      this->tasks_info[TASK_ID__ORN].solutions.emplace_back((a|(~b)));
      this->tasks_info[TASK_ID__ORN].solutions.emplace_back((b|(~a)));
    };
    // AND Task
    Task & and_task = tasks_info[TASK_ID__AND];
    and_task.task = "AND";
    and_task.id = TASK_ID__AND;
    and_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__AND].solutions.resize(0);
      this->tasks_info[TASK_ID__AND].solutions.emplace_back(a&b);
    };
    // OR Task
    Task & or_task = tasks_info[TASK_ID__OR];
    or_task.task = "OR";
    or_task.id = TASK_ID__OR;
    or_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__OR].solutions.resize(0);
      this->tasks_info[TASK_ID__OR].solutions.emplace_back(a|b);
    };
    // ANDN Task
    Task & andn_task = tasks_info[TASK_ID__ANDN];
    andn_task.task = "ANDN";
    andn_task.id = TASK_ID__ANDN;
    andn_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__ANDN].solutions.resize(0);
      this->tasks_info[TASK_ID__ANDN].solutions.emplace_back((a&(~b)));
      this->tasks_info[TASK_ID__ANDN].solutions.emplace_back((b&(~a)));
    };
    // NOR Task
    Task & nor_task = tasks_info[TASK_ID__NOR];
    nor_task.task = "NOR";
    nor_task.id = TASK_ID__NOR;
    nor_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__NOR].solutions.resize(0);
      this->tasks_info[TASK_ID__NOR].solutions.emplace_back(~(a|b));
    };
    // XOR Task
    Task & xor_task = tasks_info[TASK_ID__XOR];
    xor_task.task = "XOR";
    xor_task.id = TASK_ID__XOR;
    xor_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__XOR].solutions.resize(0);
      this->tasks_info[TASK_ID__XOR].solutions.emplace_back(a^b);
    };
    // EQU Task
    Task & equ_task = tasks_info[TASK_ID__EQU];
    equ_task.task = "EQU";
    equ_task.id = TASK_ID__EQU;
    equ_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__EQU].solutions.resize(0);
      this->tasks_info[TASK_ID__EQU].solutions.emplace_back(~(a^b));
    };
    // ECHO Task
    Task & echo_task = tasks_info[TASK_ID__ECHO];
    echo_task.task = "ECHO";
    echo_task.id = TASK_ID__ECHO;
    echo_task.calc_solutions = [this](task_input_t a, task_input_t b) {
      this->tasks_info[TASK_ID__ECHO].solutions.resize(0);
      this->tasks_info[TASK_ID__ECHO].solutions.emplace_back(a);
      this->tasks_info[TASK_ID__ECHO].solutions.emplace_back(b);
    };

    // const int a = MAX_TASK_INPUT;
    // const int b = MIN_TASK_INPUT;
    // for (size_t i = 0; i < tasks_info.size(); ++i) {
    //   tasks_info[i].calc_solutions(a, b);
    //   std::cout << "TASK: " << tasks_info[i].task << std::endl;
    //   std::cout << "  Solutions:";
    //   for (size_t k = 0; k < tasks_info[i].solutions.size(); ++k) {
    //     std::cout << "  " << tasks_info[i].solutions[k];
    //   } std::cout << std::endl;
    // }
    //
    // exit(-1);

    // Make the world.
    world = emp::NewPtr<world_t>(random, "ChgEnv-World");
    world->Reset();
    // Configure the world.
    world->SetWellMixed(true);
    world->SetMutFun([this](Agent & agent, emp::Random & rnd) { return this->Mutate(agent, rnd); });
    if (FITNESS_CALC_TYPE == FIT_TYPE__MIN) {
      world->SetFitFun([this](Agent & agent) { return this->CalcFitness__MIN(agent); });
    } else if (FITNESS_CALC_TYPE == FIT_TYPE__AVG) {
      world->SetFitFun([this](Agent & agent) { return this->CalcFitness__AVG(agent); });
    } else {
      std::cout << "Don't recognize fitness calc type. " << std::endl;
      exit(-1);
    }


    // Create empty instruction/event libraries.
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();

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

    if (PROBLEM == PROBLEM_ID__TASKS) {
      CHANGING_ENVIRONMENT = false;
      score_agent = [this](Agent & agent, size_t i) { return Score__TasksProb(agent, i); };
      // Add TASKS problem-specific instructions.
      inst_lib->AddInst("Load-1", [this](hardware_t & hw, const inst_t & inst) {
        state_t & state = hw.GetCurState();
        state.SetLocal(inst.args[0], this->task_inputs[load_id]); // Load input.
        this->load_id += 1; if (this->load_id >= this->task_inputs.size()) this->load_id = 0; // Update load ID.
      }, 1, "WM[ARG1] = Next task input.");
      inst_lib->AddInst("Load-2", [this](hardware_t & hw, const inst_t & inst) {
        state_t & state = hw.GetCurState();
        state.SetLocal(inst.args[0], this->task_inputs[0]);
        state.SetLocal(inst.args[1], this->task_inputs[1]);
      }, 2, "WM[ARG1] = TASKIN[0]; WM[ARG2]=TASKIN[1]");
      inst_lib->AddInst("Submit", [this](hardware_t & hw, const inst_t & inst) { this->Inst_Submit(hw, inst); }, 1, "Submit WM[ARG1] as potential task solution.");

    } else if (PROBLEM == PROBLEM_ID__CHANGING_ENV_WITH_TASKS) {
      CHANGING_ENVIRONMENT = true;
      score_agent = [this](Agent & agent, size_t i) { return Score__ChgEnvWithTasksProg(agent, i); };
      // Add problem-specific instructions.
      // Add conditional submit instruction.
      inst_lib->AddInst("Load-1", [this](hardware_t & hw, const inst_t & inst) {
        state_t & state = hw.GetCurState();
        state.SetLocal(inst.args[0], this->task_inputs[load_id]); // Load input.
        this->load_id += 1; if (this->load_id >= this->task_inputs.size()) this->load_id = 0; // Update load ID.
      }, 1, "WM[ARG1] = Next task input.");
      inst_lib->AddInst("Load-2", [this](hardware_t & hw, const inst_t & inst) {
        state_t & state = hw.GetCurState();
        state.SetLocal(inst.args[0], this->task_inputs[0]);
        state.SetLocal(inst.args[1], this->task_inputs[1]);
      }, 2, "WM[ARG1] = TASKIN[0]; WM[ARG2]=TASKIN[1]");
      inst_lib->AddInst("Submit", [this](hardware_t & hw, const inst_t & inst) { this->Inst_Submit__COND(hw, inst); }, 1, "Submit WM[ARG1] as potential task solution.");

    } else if (PROBLEM == PROBLEM_ID__CHANGING_ENV) {
      CHANGING_ENVIRONMENT = true;
      score_agent = [this](Agent & agent, size_t i) { return Score__ChgEnvProb(agent, i); };

    } else {
      std::cout << "Do not recognize requested problem!" << std::endl;
      exit(-1);
    }

    // Setup a changing environment.
    if (CHANGING_ENVIRONMENT) {
      // Generate a SetState instruction for every possible environment state.
      for (int i = 0; i < ENVIRONMENT_STATES; ++i) {
        inst_lib->AddInst("SetState" + emp::to_string(i),
          [i](hardware_t & hw, const inst_t & inst){
            hw.SetTrait(TRAIT_ID__STATE, i);
          }, 0, "Set internal state to " + emp::to_string(i));
      }

      // Does the environment generate signals?
      //  - If so, register the appropriate event signals.
      if (ENV_SIGNALS) {
        if (ANALYZE_MODE && TEASER_EVENTS) {
          // Use teaser events.
          event_lib->AddEvent("EnvSignal", HandleEvent__EnvSignal_IMP, "");
          event_lib->RegisterDispatchFun("EnvSignal", DispatchEvent__EnvSignal_IMP);
        } else {
          event_lib->AddEvent("EnvSignal", HandleEvent__EnvSignal_ED, "");
          event_lib->RegisterDispatchFun("EnvSignal", DispatchEvent__EnvSignal_ED);
        }
      } else {
        event_lib->AddEvent("EnvSignal", HandleEvent__EnvSignal_IMP, "");
        event_lib->RegisterDispatchFun("EnvSignal", DispatchEvent__EnvSignal_IMP);
      }

      // Are agents able to actively sense (poll) the environment?
      if (ACTIVE_SENSING) {
        if (ANALYZE_MODE && TEASER_SENSORS) {
          // Use teaser sensors.
          for (int i = 0; i < ENVIRONMENT_STATES; ++i) {
            inst_lib->AddInst("SenseState" + emp::to_string(i),
              [this, i](hardware_t & hw, const inst_t & inst) { return; },
              0, "Sense if current environment state is " + emp::to_string(i));
          }
        } else {
          for (int i = 0; i < ENVIRONMENT_STATES; ++i) {
            inst_lib->AddInst("SenseState" + emp::to_string(i),
              [this, i](hardware_t & hw, const inst_t & inst) {
                state_t & state = hw.GetCurState();
                state.SetLocal(inst.args[0], this->env_state==i);
              }, 0, "Sense if current environment state is " + emp::to_string(i));
          }
        }
      } else { // Make bloat instruction set.
        for (int i = 0; i < ENVIRONMENT_STATES; ++i) {
          inst_lib->AddInst("SenseStateNop" + emp::to_string(i),
            [this, i](hardware_t & hw, const inst_t & inst) { return; }, 0,
            "Sense if current environment state is " + emp::to_string(i));
        }
      }
    }

    // Configure evaluation hardware.
    eval_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);
    eval_hw->SetMinBindThresh(HW_MIN_BIND_THRESH);
    eval_hw->SetMaxCores(HW_MAX_CORES);
    eval_hw->SetMaxCallDepth(HW_MAX_CALL_DEPTH);

    // Setup the data/systematics output file(s).
    if (!ANALYZE_MODE) {
      auto & sys_file = world->SetupSystematicsFile(DATA_DIRECTORY + "systematics.csv");
      sys_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
      auto & fit_file = world->SetupFitnessFile(DATA_DIRECTORY + "fitness.csv");
      fit_file.SetTimingRepeat(FITNESS_INTERVAL);
    } else { // In analysis mode.
      // Hacking in these analyses
      if (TEASER_SENSORS && !TEASER_EVENTS) {
        analysis_scores_fname = DATA_DIRECTORY + "no_sensors.csv";
      } else if (TEASER_EVENTS && !TEASER_SENSORS) {
        analysis_scores_fname = DATA_DIRECTORY + "no_events.csv";
      } else if (TEASER_EVENTS && TEASER_SENSORS) {
        analysis_scores_fname = DATA_DIRECTORY + "no_sensors_no_events.csv";
      } else {
        analysis_scores_fname = DATA_DIRECTORY + "fdom.csv";
      }
    }
  }

  ~LogicOperationsExp() {
    eval_hw.Delete();
    world.Delete();
    inst_lib.Delete();
    event_lib.Delete();
    random.Delete();
  }

  /// Run in experiment mode. Either load individual ancestor or an entire population.
  /// After initializing the population, run evolution for the specified number of generations.
  void RunExperiment() {
    std::cout << "\nRunning experiment...\n" << std::endl;
    // Are we resuming from an existing population, or are we starting anew?
    if (RUN_FROM_EXISTING_POP) {
      // Configure population from the specified existing population.
      //  NOTE: This code makes assumptions about the way the existing population is stored and how many .gp files there are.
      //        -- Assumption is that the population was output by a Snapshot function from this experiment.
      for (size_t i = 0; i < POP_SIZE; ++i) {
        program_t prog(inst_lib);
        std::string prog_fpath = EXISTING_POP_LOC + "prog_" + emp::to_string((int)i) + ".gp";
        std::ifstream prog_fstream(prog_fpath);
        if (!prog_fstream.is_open()) {
          std::cout << "Failed to open program file: " << prog_fpath << std::endl;
          exit(-1);
        }
        prog.Load(prog_fstream);
        world->Inject(prog, 1);
      }
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
    double best_score = 0;
    // Evolution!
    for (size_t ud = 0; ud <= GENERATIONS; ++ud) {
      // Evaluate each agent.
      best_score = 0;
      for (size_t id = 0; id < world->GetSize(); ++id) {
        Agent & agent = world->GetOrg(id);
        agent.env_matches_by_trial.resize(TRIAL_CNT, 0);
        agent.task_completions_by_trial.resize(TRIAL_CNT, emp::vector<size_t>(TASK_CNT, 0));
        agent.task_credits_by_trial.resize(TRIAL_CNT, emp::vector<size_t>(TASK_CNT, 0));
        agent.scores_by_trial.resize(TRIAL_CNT, 0);

        LoadHWProgram(world->GetGenomeAt(id)); // Load agent's program into evaluation hardware.

        // Evaluate a single agent (for some number of trials).
        for (size_t trialID = 0; trialID < TRIAL_CNT; ++trialID) {
          env_state = -1;  // Reset the environment state.
          ResetTasks();    // Reset tasks.
          if (trialID > 0) {
            ResetHW();
          }
          // Run the hardware for some amount of time.
          for (eval_update = 0; eval_update < EVAL_TIME; ++eval_update) {
            if (CHANGING_ENVIRONMENT && (env_state == -1 || random->P(ENVIRONMENT_CHG_PROB))) {
              // Change the environment to a random state!
              env_state = random->GetUInt(ENVIRONMENT_STATES);
              // Trigger environment state event.
              eval_hw->TriggerEvent("EnvSignal", env_state_tags[env_state]);
            }
            // Run hardware for a time step.
            eval_hw->SingleProcess();
            // Does hardware state match environment state?
            if (eval_hw->GetTrait(TRAIT_ID__STATE) == env_state) {
              agent.env_matches_by_trial[trialID] += 1;
            }
          }
          // Update agent's task completion info.
          for (size_t i = 0; i < tasks_info.size(); ++i) {
            agent.task_completions_by_trial[trialID][i] = tasks_info[i].completed;
            agent.task_credits_by_trial[trialID][i] = tasks_info[i].credited;
          }
          // Compute score
          agent.scores_by_trial[trialID] = score_agent(agent, trialID);
        }
        // Compute some relevant information about performance.
        double fitness = CalcFitness__MIN(agent);
        if (fitness > best_score) { best_score = fitness; }
      }

      // Selection
      // Keep the best program around.
      emp::EliteSelect(*world, 1, 1);
      // Run a tournament for the rest.
      emp::TournamentSelect(*world, 4, POP_SIZE - 1);

      // Print out in-run summary stats.
      std::cout << "Update " << world->GetUpdate();
      std::cout << "  Max score: " << best_score << std::endl;

      // Update the world.
      world->Update();

      // Mutate everyone but the first (elite) agent.
      world->DoMutations(1);

      // Population snapshot?
      if (ud % POP_SNAPSHOT_INTERVAL == 0) SnapshotSF(ud);
    }
  }

  void RunAnalysis() {
    std::cout << "\nRunning analysis...\n" << std::endl;
    switch (ANALYSIS) {
      case 0: {
        // Configure analysis program.
        program_t analyze_prog(inst_lib);
        std::ifstream analyze_fstream(ANALYZE_AGENT_FPATH);
        if (!analyze_fstream.is_open()) {
          std::cout << "Failed to open analysis program file(" << ANCESTOR_FPATH << "). Exiting..." << std::endl;
          exit(-1);
        }
        analyze_prog.Load(analyze_fstream);
        std::cout << " --- Analysis program: ---" << std::endl;
        analyze_prog.PrintProgramFull();
        std::cout << " -------------------------" << std::endl;

        Agent agent(analyze_prog);
        agent.env_matches_by_trial.resize(TRIAL_CNT, 0);
        agent.task_completions_by_trial.resize(TRIAL_CNT, emp::vector<size_t>(TASK_CNT, 0));
        agent.task_credits_by_trial.resize(TRIAL_CNT, emp::vector<size_t>(TASK_CNT, 0));
        agent.scores_by_trial.resize(TRIAL_CNT, 0);

        LoadHWProgram(analyze_prog); // Load agent's program into evaluation hardware.
        // Evaluate a single agent (for some number of trials).
        for (size_t trialID = 0; trialID < TRIAL_CNT; ++trialID) {
          env_state = -1;  // Reset the environment state.
          ResetTasks();    // Reset tasks.
          if (trialID > 0) {
            ResetHW();
          }
          std::cout << "================ TRIAL " << trialID << "================" << std::endl;

          std::cout << "--- TASK SOLUTIONS --" << std::endl;
          for (size_t i = 0; i < tasks_info.size(); ++i) {
            std::cout << "TASK: " << tasks_info[i].task << std::endl;
            std::cout << "  Solutions:";
            for (size_t k = 0; k < tasks_info[i].solutions.size(); ++k) {
              std::cout << "  " << tasks_info[i].solutions[k];
            } std::cout << std::endl;
          }
          std::cout << " -------------------------" << std::endl;
          std::cout << "\n\nBEGIN AGENT EVALUATION" << std::endl;
          eval_hw->PrintState();
          // Run the hardware for some amount of time.
          for (eval_update = 0; eval_update < EVAL_TIME; ++eval_update) {
            std::cout << "================= TIME: " << eval_update << " =================" << std::endl;
            if (CHANGING_ENVIRONMENT && (env_state == -1 || random->P(ENVIRONMENT_CHG_PROB))) {
              std::cout << "  ENV CHG: " << env_state << " --> ";
              // Change the environment to a random state!
              env_state = random->GetUInt(ENVIRONMENT_STATES);
              std::cout << env_state << std::endl;
              // Trigger environment state event.
              eval_hw->TriggerEvent("EnvSignal", env_state_tags[env_state]);
            }
            std::cout << "Environment state: " << env_state << std::endl;
            // Run hardware for a time step.
            eval_hw->SingleProcess();
            eval_hw->PrintState();
            // Does hardware state match environment state?
            if (eval_hw->GetTrait(TRAIT_ID__STATE) == env_state) {
              agent.env_matches_by_trial[trialID] += 1;
            }
          }
          // Update agent's task completion info.
          for (size_t i = 0; i < tasks_info.size(); ++i) {
            agent.task_completions_by_trial[trialID][i] = tasks_info[i].completed;
            agent.task_credits_by_trial[trialID][i] = tasks_info[i].credited;
          }
          // Compute score
          agent.scores_by_trial[trialID] = score_agent(agent, trialID);
          std::cout << "\nTrial score: " << agent.scores_by_trial[trialID] << "\n" << std::endl;
          for (size_t i = 0; i < TASK_CNT; ++i) {
            std::cout << "  Task: " << tasks_info[i].task << " ("<< tasks_info[i].id <<")" << std::endl;
            std::cout << "    Comp: " << tasks_info[i].completed << std::endl;
            std::cout << "      Comp: TS:";
            for (size_t k = 0; k < tasks_info[i].comp_time_stamps.size(); ++k) std::cout << " " << tasks_info[i].comp_time_stamps[k];
            std::cout << std::endl;
            std::cout << "    Cred: " << tasks_info[i].credited << std::endl;
            std::cout << "      Cred TS:";
            for (size_t k = 0; k < tasks_info[i].cred_time_stamps.size(); ++k) std::cout << " " << tasks_info[i].cred_time_stamps[k];
            std::cout << std::endl;
          }
        }

        std::cout << "\n\n\n\nAGENT EVALUATION SUMMARY" << std::endl;
        std::cout << "Agent fitness (min): " << CalcFitness__MIN(agent) << std::endl;
        std::cout << "Agent fitness (avg): " << CalcFitness__AVG(agent) << std::endl;

        std::cout << "Agent scores:";
        for (size_t i = 0; i < agent.scores_by_trial.size(); ++i) std::cout << "  " << agent.scores_by_trial[i];
        std::cout << std::endl;

        std::cout << "Env match scores:";
        for (size_t i = 0; i < agent.env_matches_by_trial.size(); ++i) std::cout << "  " << agent.env_matches_by_trial[i];
        std::cout << std::endl;

        std::cout << "Tasks summary: " << std::endl;
        for (size_t ti = 0; ti < TRIAL_CNT; ++ti) {
          std::cout << " --- TRIAL " << ti << " --- " << std::endl;
          for (size_t i = 0; i < TASK_CNT; ++i) {
            std::cout << "  Task: " << tasks_info[i].task << " ("<< tasks_info[i].id <<")" << std::endl;
            std::cout << "    Comp: " << agent.task_completions_by_trial[ti][i] << std::endl;
            std::cout << "    Cred: " << agent.task_credits_by_trial[ti][i] << std::endl;
          }
        }
        break;
      }
      default:
        std::cout << "Unrecognized analysis." << std::endl;
        break;
    }
  }

  /// Run the experiment!
  void Run() {
    if (ANALYZE_MODE) {
      RunAnalysis();
    } else {
      RunExperiment();
    }
  }

  void ResetHW() {
    eval_hw->ResetHardware(); // Reset hardware.
    eval_hw->SetTrait(TRAIT_ID__STATE, -1);
    eval_hw->SpawnCore(0, memory_t(), true);
  }

  void LoadHWProgram(const program_t & program) {
    eval_hw->SetProgram(program);
    ResetHW();
  }

  void ResetTasks() {
    const task_input_t a = random->GetUInt(MIN_TASK_INPUT, MAX_TASK_INPUT);
    const task_input_t b = random->GetUInt(MIN_TASK_INPUT, MAX_TASK_INPUT);
    for (size_t i = 0; i < tasks_info.size(); ++i) {
      Task & task = tasks_info[i];
      task.completed = 0;
      task.credited = 0;
      task.comp_time_stamps.resize(0);
      task.cred_time_stamps.resize(0);
      task.calc_solutions(a, b);
    }
    load_id = 0;
    task_inputs[0] = a;
    task_inputs[1] = b;
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

  /// Fitness function.
  double CalcFitness__MIN(Agent & agent) {
    if (!agent.scores_by_trial.size()) return 0.0;
    double val = agent.scores_by_trial[0];
    for (size_t i = 0; i < agent.scores_by_trial.size(); ++i) {
      if (agent.scores_by_trial[i] < val) val = agent.scores_by_trial[i];
    }
    return val;
  }

  double CalcFitness__AVG(Agent & agent) {
    if (!agent.scores_by_trial.size()) return 0.0;
    double val = 0.0;
    for (size_t i = 0; i < agent.scores_by_trial.size(); ++i) {
      val += agent.scores_by_trial[i];
    }
    return val / agent.scores_by_trial.size();
  }

  double Score__ChgEnvProb(Agent & agent, size_t trialID) {
    return (double)agent.env_matches_by_trial[trialID];
  }

  double Score__TasksProb(Agent & agent, size_t trialID) {
    int score = 0;
    for (size_t i = 0; i < TASK_CNT; ++i) {
      if (tasks_info[i].completed > 0) score += 1;
    }
    if (score == TASK_CNT) { // If all tasks were completed, when was final task done?
      int all_tasks_update = -1;
      for (size_t i = 0; i < TASK_CNT; ++i) {
        if ((int)tasks_info[i].comp_time_stamps[0] > all_tasks_update) {
          all_tasks_update = tasks_info[i].comp_time_stamps[0];
        }

      }
      score += (EVAL_TIME - all_tasks_update);
    }
    return (double)score;
  }

  double Score__ChgEnvWithTasksProg(Agent & agent, size_t trialID) {
    int score = 0;
    for (size_t i = 0; i < TASK_CNT; ++i) {
      if (tasks_info[i].credited > 0) score += 1;
    }
    if (score == TASK_CNT) { // If all tasks were completed, when was final task done?
      int all_tasks_update = -1;
      for (size_t i = 0; i < TASK_CNT; ++i) {
        if ((int)tasks_info[i].cred_time_stamps[0] > all_tasks_update)
          all_tasks_update = tasks_info[i].cred_time_stamps[0];
      }
      score += (EVAL_TIME - all_tasks_update);
    }
    return (double)score;
  }

  /// This function takes a snapshot of the world.
  /// Prints out the full programs of every agent in the population in a form that can be accurately reloaded.
  void Snapshot(size_t update) {
    std::string snapshot_dir = DATA_DIRECTORY + "pop_" + emp::to_string((int)update);
    mkdir(snapshot_dir.c_str(), ACCESSPERMS);
    // For each program in the population, dump the full program description.
    for (size_t i = 0; i < world->GetSize(); ++i) {
      Agent & agent = world->GetOrg(i);
      std::ofstream prog_ofstream(snapshot_dir + "/prog_" + emp::to_string((int)i) + ".gp");
      agent.program.PrintProgramFull(prog_ofstream);
      prog_ofstream.close();
    }
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


  // Events.
  static void HandleEvent__EnvSignal_ED(hardware_t & hw, const event_t & event) {
    hw.SpawnCore(event.affinity, hw.GetMinBindThresh(), event.msg);
  }

  static void HandleEvent__EnvSignal_IMP(hardware_t & hw, const event_t & event) {
    // Do nothing.
    return;
  }

  static void DispatchEvent__EnvSignal_ED(hardware_t & hw, const event_t & event) {
    hw.QueueEvent(event);
  }

  static void DispatchEvent__EnvSignal_IMP(hardware_t & hw, const event_t & event) {
    // Do nothing.
    return;
  }

  // Instructions
  /// Instruction:
  /// Description:
  void Inst_SenseEnv(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    state.SetLocal(inst.args[0], env_state);
  }

  /// Instruction: Submit
  /// Description: Submit WM[ARG1] as solution to a task.
  ///  - Check WM[ARG1] against all possible solutions on all tasks.
  ///  - Give credit where credit is due.
  void Inst_Submit(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    const task_input_t sol = (task_input_t)state.GetLocal(inst.args[0]);
    for (size_t i = 0; i < tasks_info.size(); ++i) {
      Task & task = tasks_info[i];
      for (size_t s = 0; s < task.solutions.size(); ++s) {
        if (task.solutions[s] == sol) {
          task.completed += 1;
          task.comp_time_stamps.emplace_back(eval_update);
          task.credited += 1;
          task.cred_time_stamps.emplace_back(eval_update);
        }
      }
    }
  }

  /// Instruction: Submit
  /// Description: Submit WM[ARG1] as solution to a task. Only give credit if
  ///   internal state matches environment state.
  ///  - Check WM[ARG1] against all possible solutions on all tasks.
  ///  - Give credit where credit is due if internal state == env state.
  void Inst_Submit__COND(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    const task_input_t sol = (task_input_t)state.GetLocal(inst.args[0]);
    for (size_t i = 0; i < tasks_info.size(); ++i) {
      Task & task = tasks_info[i];
      for (size_t s = 0; s < task.solutions.size(); ++s) {
        if (task.solutions[s] == sol) {
          task.completed += 1;
          task.comp_time_stamps.emplace_back(eval_update);
          if (hw.GetTrait(TRAIT_ID__STATE) == env_state) {
            task.credited += 1;
            task.cred_time_stamps.emplace_back(eval_update);
          }
        }
      }
    }
  }

  /// Instruction: Fork
  /// Description: Fork thread with local memory as new thread's input buffer.
  static void Inst_Fork(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    hw.SpawnCore(inst.affinity, hw.GetMinBindThresh(), state.local_mem);
  }

  static void Inst_Nand(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    const task_input_t a = (task_input_t)state.GetLocal(inst.args[0]);
    const task_input_t b = (task_input_t)state.GetLocal(inst.args[1]);
    state.SetLocal(inst.args[2], ~(a&b));
  }

};

int main(int argc, char * argv[]) {
  // Read configs.
  std::string config_fname = "configs.cfg";
  auto args = emp::cl::ArgManager(argc, argv);
  LogicOperationsConfig config;
  config.Read(config_fname);
  if (args.ProcessConfigOptions(config, std::cout, config_fname, "changing_environment-config.h") == false) exit(0);
  if (args.TestUnknown() == false) exit(0);

  std::cout << "==============================" << std::endl;
  std::cout << "|    How am I configured?    |" << std::endl;
  std::cout << "==============================" << std::endl;
  config.Write(std::cout);
  std::cout << "==============================\n" << std::endl;

  // Create experiment with configs, then run it!
  LogicOperationsExp e(config);
  e.Run();
}
