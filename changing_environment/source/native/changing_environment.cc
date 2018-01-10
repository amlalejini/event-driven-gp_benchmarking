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
#include "hardware/EventDrivenGP.h"
#include "Evo/World.h"
#include "changing_environment-config.h"

constexpr size_t AFFINITY_WIDTH = 16; ///< How many bits should affinities be?
// Hardware trait indices.
constexpr size_t TRAIT_ID__STATE = 0; ///< Agent state trait.

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
  /// Agents are defined by a program
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
  size_t EVAL_TIME;
  std::string ANCESTOR_FPATH; ///< File path to ancestor program description.

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

  emp::Ptr<emp::Random> random;
  emp::Ptr<world_t> world;

  emp::Ptr<inst_lib_t> inst_lib;
  emp::Ptr<event_lib_t> event_lib;

  emp::Ptr<hardware_t> eval_hw;

public:
  ChangingEnvironmentExp(const ChangingEnvironmentConfig & config) {
    // Fill out experiment parameters with config settings.
    DEBUG_MODE = config.DEBUG_MODE();
    RANDOM_SEED = config.RANDOM_SEED();
    GENERATIONS = config.GENERATIONS();
    POP_SIZE = config.POP_SIZE();
    EVAL_TIME = config.EVAL_TIME();
    ANCESTOR_FPATH = config.ANCESTOR_FPATH();
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

    // Setup the output directory.
    mkdir(DATA_DIRECTORY.c_str(), ACCESSPERMS);
    if (DATA_DIRECTORY.back() != '/') DATA_DIRECTORY += '/';

    // clean up existing population location directory.
    if (EXISTING_POP_LOC.back() != '/') EXISTING_POP_LOC += '/';

    // Make our random number generator.
    random = emp::NewPtr<emp::Random>(RANDOM_SEED);

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

    // TODO: custom instructions
    // TODO: events
    if (EVENT_DRIVEN) {

    }
    if (ACTIVE_SENSING) {

    }

    // Configure the world.
    world->SetWellMixed(true);
    world->SetMutFun([this](Agent & agent, emp::Random & rnd) { return this->Mutate(agent, rnd); });
    world->SetFitFun([this](Agent & agent) { return this->CalcFitness(agent); });

    // Configure evaluation hardware.
    eval_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);

    // Configure ancestor.(todo)
    // std::ifstream ancestor_fstream(ANCESTOR_FPATH);
    // if (!ancestor_fstream.is_open()) {
    //   std::cout << "Failed to open ancestor program file. Exiting..." << std::endl;
    //   exit(-1);
    // }
    // ancestor.Load(ancestor_fstream);
    // ancestor.SetMinBindThresh(HW_MIN_BIND_THRESH);
    // ancestor.SetMaxCores(HW_MAX_CORES);
    // ancestor.SetMaxCallDepth(HW_MAX_CALL_DEPTH);

    // Setup the data/systematics output file(s).
    auto & sys_file = world->SetupSystematicsFile(DATA_DIRECTORY + "systematics.csv");
    sys_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);
    auto & fit_file = world->SetupFitnessFile(DATA_DIRECTORY + "fitness.csv");
    fit_file.SetTimingRepeat(FITNESS_INTERVAL);
    // world->SetupPopulationFile(DATA_DIRECTORY + "population.csv").SetTimingRepeat(POPULATION_INTERVAL);
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
    for (size_t ud = 0; ud <= GENERATIONS; ++ud) {
      // Evaluate each agent.
      best_score = 0;
      best_agent = 0;
      for (size_t id = 0; id < world->GetSize(); ++id) {
        Agent & agent = world->GetOrg(id);
        agent.score = 0; // Reset agent's score to 0.
        eval_hw->SetProgram(world->GetGenomeAt(id)); // Load agent's program into evaluation hardware.
        // Run the hardware for some amount of time.
        for (size_t t = 0; t < EVAL_TIME; ++t) {
          // Change environment?
          // TODO
          // Run hardware for a time step.
          eval_hw->SingleProcess();
          // Does hardware state match environment state?
          // TODO; if so, agent.score += 1
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
      if (ud % POP_SNAPSHOT_INTERVAL == 0) Snapshot(ud);
    }
  }


  /// TODO:
  void RunAnalysis() {
    std::cout << "Running analysis..." << std::endl;
    //TODO!
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
    std::string prog_filename;
    mkdir(snapshot_dir.c_str(), ACCESSPERMS);
    // For each program in the population, dump the full program description.
    for (size_t i = 0; i < world->GetSize(); ++i) {
      Agent & agent = world->GetOrg(i);
      std::ofstream prog_ofstream(snapshot_dir + "/prog_" + emp::to_string((int)i) + ".gp");
      agent.program.PrintProgramFull(prog_ofstream);
      prog_ofstream.close();
    }
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
