// This is the main function for the NATIVE version of this project.

#include <iostream>
#include <string>
#include <deque>
#include <unordered_set>
#include <utility>
#include <fstream>
#include <sys/stat.h>

#include "base/Ptr.h"
#include "base/vector.h"
#include "config/ArgManager.h"
#include "tools/Random.h"
#include "tools/random_utils.h"
#include "tools/math.h"
#include "tools/string_utils.h"
#include "hardware/EventDrivenGP.h"
#include "Evo/World.h"
#include "changing_environment-config.h"

constexpr size_t AFFINITY_WIDTH = 16; ///< How many bits should affinities be?
// Hardware trait indices.
constexpr size_t TRAIT_ID__STATE = 0; ///< Agent state trait.

// Available environment affinity strings.
emp::vector<std::string> env_affinity_strs = {"0000000000000000",
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

size_t MAX_ENV_STATES = env_affinity_strs.size(); ///< Maximum possible number of environment states.

/// Class to manage a changing environment Signal GP benchmark experiment.
///  - Will be configured based on given configs (i.e. treatment parameters).
class ChangingEnvironmentExp {
public:
  using hardware_t = emp::EventDrivenGP_AW<AFFINITY_WIDTH>;
  using program_t = hardware_t::Program;
  using state_t = hardware_t::State;
  using inst_t = hardware_t::inst_t;
  using inst_lib_t = hardware_t::inst_lib_t;
  using event_t = hardware_t::event_t;
  using event_lib_t = hardware_t::event_lib_t;
  using memory_t = hardware_t::memory_t;
  using affinity_t = hardware_t::affinity_t;

  /// Struct to keep track of agents.
  /// Agents are defined by a program and a score.
  struct Agent {
    program_t program;
    size_t score;

    Agent(const program_t & _p) :
      program(_p),
      score(0)
    { ; }

    Agent(Agent && in)
      : program(in.program),
        score(in.score)
    { ; }

    Agent(const Agent & in)
      : program(in.program),
        score(in.score)
    { ; }

    /// Required by Empirical's World.h.
    program_t & GetGenome() { return program; }
  };

  using world_t = emp::World<Agent>;

protected:
  // == Configurable experiment parameters. ==
  bool DEBUG_MODE;    ///< Are we in debug mode? NOTE: Currenly not in use.
  int RANDOM_SEED;    ///< Random seed to use for this experiment.
  size_t GENERATIONS; ///< How generations should the experiment run for?
  size_t POP_SIZE;    ///< Population size.
  size_t EVAL_TIME;   ///< How long should we evaluate agents for?
  std::string ANCESTOR_FPATH; ///< File path to ancestor program description.

  // Environment-specific settings.
  size_t ENVIRONMENT_STATES;        ///< Number of environment states.
  double ENVIRONMENT_CHG_PROB;      ///< Probability of environment change on update.

  // Hardware-specific settings.
  bool EVENT_DRIVEN;         ///< Do environment changes trigger events?
  bool ACTIVE_SENSING;       ///< Is active sensing enabled?
  size_t HW_MAX_CORES;       ///< Max number of hardware cores. i.e. max number of simultaneous threads of execution hardware will support.
  size_t HW_MAX_CALL_DEPTH;  ///< Max call depth of hardware unit.
  double HW_MIN_BIND_THRESH; ///< Hardware minimum binding threshold.

  // Mutation-specific settings.
  size_t PROG_MAX_FUNC_CNT;
  size_t PROG_MAX_FUNC_LEN;
  size_t PROG_MAX_ARG_VAL;
  double PER_BIT__AFFINITY_FLIP_RATE;
  double PER_INST__SUB_RATE;
  double PER_FUNC__SLIP_RATE;
  double PER_FUNC__FUNC_DUP_RATE;
  double PER_FUNC__FUNC_DEL_RATE;

  // Data output-specific settings.
  size_t SYSTEMATICS_INTERVAL;    ///< Interval to save summary statistics.
  size_t FITNESS_INTERVAL;        ///< Interval to save fitness statistics.
  size_t POPULATION_INTERVAL;
  size_t POP_SNAPSHOT_INTERVAL;   ///< Interval to take full program snapshots of population.
  std::string DATA_DIRECTORY;     ///< Directory in which to store all program output.

  // Run from pop settings.
  bool RUN_FROM_EXISTING_POP;
  std::string EXISTING_POP_LOC;

  // Analysis settings.
  bool ANALYZE_MODE;
  size_t ANALYSIS;
  std::string ANALYZE_AGENT_FPATH;
  size_t FDOM_ANALYSIS_TRIAL_CNT;
  bool TEASER_SENSORS;
  bool TEASER_EVENTS;

  emp::Ptr<emp::Random> random;
  emp::Ptr<world_t> world;

  emp::Ptr<inst_lib_t> inst_lib;
  emp::Ptr<event_lib_t> event_lib;

  emp::Ptr<hardware_t> eval_hw;

  emp::vector<affinity_t> env_state_affs; ///< Affinities associated with each environment state.

  int env_state;

  std::string analysis1_scores_fname;

public:
  ChangingEnvironmentExp(const ChangingEnvironmentConfig & config) : env_state(-1) {
    // Fill out experiment parameters with config settings.
    DEBUG_MODE = config.DEBUG_MODE();
    RANDOM_SEED = config.RANDOM_SEED();
    GENERATIONS = config.GENERATIONS();
    POP_SIZE = config.POP_SIZE();
    EVAL_TIME = config.EVAL_TIME();
    ANCESTOR_FPATH = config.ANCESTOR_FPATH();
    ENVIRONMENT_STATES = config.ENVIRONMENT_STATES();
    ENVIRONMENT_CHG_PROB = config.ENVIRONMENT_CHG_PROB();
    EVENT_DRIVEN = config.EVENT_DRIVEN();
    ACTIVE_SENSING = config.ACTIVE_SENSING();
    HW_MAX_CORES = config.HW_MAX_CORES();
    HW_MAX_CALL_DEPTH = config.HW_MAX_CALL_DEPTH();
    HW_MIN_BIND_THRESH = config.HW_MIN_BIND_THRESH();
    PROG_MAX_FUNC_CNT = config.PROG_MAX_FUNC_CNT();
    PROG_MAX_FUNC_LEN = config.PROG_MAX_FUNC_LEN();
    PROG_MAX_ARG_VAL = config.PROG_MAX_ARG_VAL();
    PER_BIT__AFFINITY_FLIP_RATE = config.PER_BIT__AFFINITY_FLIP_RATE();
    PER_INST__SUB_RATE = config.PER_INST__SUB_RATE();
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

    // Configure the environment.
    if (ENVIRONMENT_STATES > MAX_ENV_STATES) {
      std::cout << "Requested environment states exceeds maximum environment states." << std::endl;
      std::cout << "Setting ENVIRONMENT_STATES to MAX_ENV_STATES" << std::endl;
      ENVIRONMENT_STATES = MAX_ENV_STATES;
    }
    for (size_t i = 0; i < ENVIRONMENT_STATES; ++i) {
      env_state_affs.emplace_back(affinity_t());
      for (size_t c = 0; c < env_affinity_strs[i].size(); ++c) {
        if (env_affinity_strs[i][c] == '0') env_state_affs[i].Set(c, 0);
        else env_state_affs[i].Set(c, 1);
      }
    }
    std::cout << "Environment states: " << std::endl;
    for (size_t i = 0; i < env_state_affs.size(); ++i) {
      std::cout << i << ": "; env_state_affs[i].Print(); std::cout << std::endl;
    }

    // Make the world.
    world = emp::NewPtr<world_t>(random, "ChgEnv-World");
    world->Reset();

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
    // Generate a SetState instruction for every possible environment state.
    for (int i = 0; i < ENVIRONMENT_STATES; ++i) {
      inst_lib->AddInst("SetState" + emp::to_string(i),
        [i](hardware_t & hw, const inst_t & inst){
          hw.SetTrait(TRAIT_ID__STATE, i);
        }, 0, "Set internal state to " + emp::to_string(i));
    }
    // Add events as appropriate.
    if (EVENT_DRIVEN) {
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

    // Configure the world.
    world->SetWellMixed(true);
    world->SetMutFun([this](Agent & agent, emp::Random & rnd) { return this->Mutate(agent, rnd); });
    world->SetFitFun([this](Agent & agent) { return this->CalcFitness(agent); });

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
    }

    // Hacking this stuff in...
    if (TEASER_SENSORS && !TEASER_EVENTS) {
      analysis1_scores_fname = "teaser_sensors.csv";
    } else if (TEASER_EVENTS && !TEASER_SENSORS) {
      analysis1_scores_fname = "teaser_events.csv";
    } else if (TEASER_EVENTS && TEASER_SENSORS) {
      analysis1_scores_fname = "full_teaser.csv";
    } else {
      analysis1_scores_fname = "fdom.csv";
    }
  }

  ~ChangingEnvironmentExp() {
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
    // Setup the evaluation hardware.

    // Population initialization done, ready to run evolution!
    double best_score = 0;
    size_t best_agent = 0;
    // emp::vector<size_t> possible_states;
    // size_t ei = 0;
    // for (size_t i = 0; i < ENVIRONMENT_STATES; ++i) possible_states.emplace_back(i);
    for (size_t ud = 0; ud <= GENERATIONS; ++ud) {
      // Evaluate each agent.
      best_score = 0;
      best_agent = 0;
      for (size_t id = 0; id < world->GetSize(); ++id) {
        Agent & agent = world->GetOrg(id);
        agent.score = 0; // Reset agent's score to 0.
        env_state = -1;  // Reset the environment state.
        // ei = 0;
        // Shuffle(*random, possible_states);
        LoadHWProgram(world->GetGenomeAt(id)); // Load agent's program into evaluation hardware.
        // Run the hardware for some amount of time.
        for (size_t t = 1; t < EVAL_TIME; ++t) {
          if (env_state == -1 || random->P(ENVIRONMENT_CHG_PROB)) {
            // Change the environment to a random state!
            env_state = random->GetUInt(ENVIRONMENT_STATES);
            // Trigger environment state event.
            eval_hw->TriggerEvent("EnvSignal", env_state_affs[env_state]);
          }
          // Run hardware for a time step.
          eval_hw->SingleProcess();
          // Does hardware state match environment state?
          if (eval_hw->GetTrait(TRAIT_ID__STATE) == env_state) {
            agent.score += 1;
          }
        }
        // Compute some relevant information about performance.
        double fitness = CalcFitness(agent);
        if (fitness > best_score) { best_score = fitness; best_agent = id; }
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
      case 0: { // Analyze a single program specified by ANALYZE AGENT
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

        LoadHWProgram(analyze_prog);

        env_state = -1;  // Reset the environment state.
        size_t match_score = 0;
        // Evaluate hardware.
        std::cout << "\n\nBEGIN AGENT EVALUATION" << std::endl;
        eval_hw->PrintState();
        for (size_t t = 1; t < EVAL_TIME; ++t) {
          std::cout << "================= TIME: " << t << " =================" << std::endl;
          if (env_state == -1 || random->P(ENVIRONMENT_CHG_PROB)) {
            std::cout << "  ENV CHG: " << env_state << " --> ";
            env_state = random->GetUInt(ENVIRONMENT_STATES);
            std::cout << env_state << std::endl;
            eval_hw->TriggerEvent("EnvSignal", env_state_affs[env_state]);
          }
          std::cout << "Environment state: " << env_state << std::endl;
          // Run hardware for single timestep.
          eval_hw->SingleProcess();
          eval_hw->PrintState();
          if (eval_hw->GetTrait(TRAIT_ID__STATE) == env_state) ++match_score;
        }
        std::cout << "\n\nAGENT EVALUATION SUMMARY" << std::endl;
        Agent agent(analyze_prog);
        agent.score = match_score;
        double score = CalcFitness(agent);
        std::cout << "  Final score: " << score << std::endl;
        break;
      }
      case 1: {
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

        LoadHWProgram(analyze_prog);

        env_state = -1;  // Reset the environment state.
        emp::vector<size_t> match_scores; match_scores.resize(FDOM_ANALYSIS_TRIAL_CNT);
        size_t match_score = 0;
        for (size_t tID = 0; tID < FDOM_ANALYSIS_TRIAL_CNT; ++tID) {
          match_score = 0;
          // Evaluate hardware.
          for (size_t t = 1; t < EVAL_TIME; ++t) {
            if (env_state == -1 || random->P(ENVIRONMENT_CHG_PROB)) {
              env_state = random->GetUInt(ENVIRONMENT_STATES);
              eval_hw->TriggerEvent("EnvSignal", env_state_affs[env_state]);
            }
            // Run hardware for single timestep.
            eval_hw->SingleProcess();
            if (eval_hw->GetTrait(TRAIT_ID__STATE) == env_state) ++match_score;
          }
          Agent agent(analyze_prog);
          agent.score = match_score;
          match_scores[tID] = CalcFitness(agent);
          ResetHW();
        }

        // Output shit.
        std::ofstream prog_ofstream("./"+analysis1_scores_fname);
        // Fill out the header.
        prog_ofstream << "trial,fitness";
        for (size_t tID = 0; tID < match_scores.size(); ++tID) {
          prog_ofstream << "\n" << tID << "," << match_scores[tID];
        }
        prog_ofstream.close();

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
  }

  void LoadHWProgram(const program_t & program) {
    ResetHW();
    eval_hw->SetProgram(program);
    eval_hw->SpawnCore(0, memory_t(), true);
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
        affinity_t & aff = program[fID].GetAffinity();
        if (rnd.P(PER_BIT__AFFINITY_FLIP_RATE)) {
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
          if (rnd.P(PER_BIT__AFFINITY_FLIP_RATE)) {
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
    }
    return mut_cnt;
  }

  /// Fitness function.
  double CalcFitness(Agent & agent) {
    return agent.score;
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
  ChangingEnvironmentConfig config;
  config.Read(config_fname);
  if (args.ProcessConfigOptions(config, std::cout, config_fname, "changing_environment-config.h") == false) exit(0);
  if (args.TestUnknown() == false) exit(0);

  std::cout << "==============================" << std::endl;
  std::cout << "|    How am I configured?    |" << std::endl;
  std::cout << "==============================" << std::endl;
  config.Write(std::cout);
  std::cout << "==============================\n" << std::endl;

  // Create experiment with configs, then run it!
  ChangingEnvironmentExp e(config);
  e.Run();
}
