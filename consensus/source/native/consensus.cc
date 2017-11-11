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

#include "consensus-config.h"

constexpr size_t AFFINITY_WIDTH = 16;    //< How many bits make up each affinity?

// Hardware trait indices. Each of these constants is an index into EventDrivenGP/Signal GP's trait vector.
constexpr size_t TRAIT_ID__LOC     = 0;  //< Trait ID that specifies hardware's location within deme grid.
constexpr size_t TRAIT_ID__DIR     = 1;  //< Trait ID that specifies hardware's orientation.
constexpr size_t TRAIT_ID__UID     = 2;  //< Trait ID that specifies hardware's unique identifer.
constexpr size_t TRAIT_ID__OPINION = 3;  //< Trait ID that specifies hardware's current opinion.

constexpr size_t NUM_NEIGHBORS = 4;      //< Number of neighboring locations for each grid location in a deme.

// Orientation idenifiers. I.e. 0 means up, 1 means left, etc.
constexpr size_t DIR_UP = 0;
constexpr size_t DIR_LEFT = 1;
constexpr size_t DIR_DOWN = 2;
constexpr size_t DIR_RIGHT = 3;

// Min and max valid unique idenifiers. Used when generating unique idenifiers for virtual hardware.
constexpr size_t MIN_UID = 1;           //< Minimum bound on hardware UID. NOTE: This should not overlap 0. A 0 is used to specify no-vote.
constexpr size_t MAX_UID = 10000;       //< Maximum bound on hardware UID.

/// Class to manage a consensus experiment.
///  - Will be configured based on treatment parameters.
class ConsensusExp {
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

  /// Struct to keep track of agents (target of evolution) for the Consensus experiment.
  /// Each agent is defined by its program (i.e. its genotype). Agent struct also contains
  /// some useful information about how the agent performed when evaluated.
  struct Agent {
    program_t program;          //< EventDrivenGP::Program (i.e. the agent's genotype).
    size_t full_consensus_time; //< How many deme updates was this agent's program able to maintain a full, valid concensus?
    size_t valid_votes;         //< After evaluation, how many votes are valid? (Range: [0:DEME SIZE])
    size_t max_consensus;       //< After evaluation, what is the largest valid consensus? (Range: [0:DEME SIZE])

    Agent(const program_t & _p)
      : program(_p),
        full_consensus_time(0),
        valid_votes(0), max_consensus(0)
    { ; }

    Agent(Agent && in)
      : program(in.program),
        full_consensus_time(in.full_consensus_time),
        valid_votes(in.valid_votes),
        max_consensus(in.max_consensus)
    { ; }

    Agent(const Agent & in)
      : program(in.program),
        full_consensus_time(in.full_consensus_time),
        valid_votes(in.valid_votes),
        max_consensus(in.max_consensus)
    { ; }

    /// Required by Empirical's World.h.
    program_t & GetGenome() { return program; }
  };

  /// Struct to keep track of demes for the Consensus experiment.
  /// Demes are toroidal grids of virtual hardware (EventDrivenGP/Signal GP).
  /// Hardware programs are homogeneous (i.e. each hardware unit has the exact same program loaded onto it).
  struct Deme {
    using grid_t = emp::vector<hardware_t>;
    using inbox_t = std::deque<event_t>;

    grid_t grid;            //< Toroidal grid of hardware (stored in a 1D vector).
    size_t width;           //< Width of the toroidal grid.
    size_t height;          //< Height of the toroidal grid.
    size_t inbox_capacity;  //< Max inbox capacity of hardware (when using inbox message delivery).

    emp::vector<size_t> schedule; //< Utility vector to store order to give each hardware in the deme a CPU cycle on a single deme update.
    emp::vector<inbox_t> inboxes; //< Inbox for each hardware in the deme.

    std::unordered_set<size_t> uids;             //< Set of UIDs currently in use.
    std::unordered_multiset<size_t> valid_votes; //< Maintains the number of valid votes present in the deme at the end of the most recent deme-update.
    size_t max_vote_cnt;                         //< Highest vote count for any single valid UID.

    emp::Ptr<emp::Random> rnd;  //< Pointer to a random number generator. NOTE: Deme struct is not responsible for pointer cleanup.
    program_t germ_prog;        //< Current program loaded onto deme hardware. Initialized to be empty.

    /// Deme construction requires:
    ///  _rnd: pointer to random number generator (deme does not claim cleanup responsibility)
    ///  _w: deme width
    ///  _h: deme height
    ///  _ibox_cap: inbox capacity
    ///  _ilib: pointer to the instruction library that hardware will use. NOTE: Deme struct is not responsible for pointer cleanup.
    ///  _elib: pointer to event library that hardware will use. NOTE: Deme struct is not responsible for pointer cleanup.
    Deme(emp::Ptr<emp::Random> _rnd, size_t _w, size_t _h, size_t _ibox_cap, emp::Ptr<inst_lib_t> _ilib, emp::Ptr<event_lib_t> _elib)
      : grid(), width(_w), height(_h), inbox_capacity(_ibox_cap),
        schedule(width*height), inboxes(width*height),
        uids(), valid_votes(), max_vote_cnt(0),
        rnd(_rnd), germ_prog(_ilib)
    {
      // Fill out the grid with hardware.
      for (size_t i = 0; i < width * height; ++i) {
        grid.emplace_back(_ilib, _elib, rnd);
        schedule[i] = i;
        uids.emplace(i+1);  // i+1 to avoid giving out a UID of zero.
        hardware_t & cpu = grid.back();
        // Set hardware traits to valid initial values.
        cpu.SetTrait(TRAIT_ID__LOC, i);
        cpu.SetTrait(TRAIT_ID__DIR, 0);       // We'll start everyone out facing up.
        cpu.SetTrait(TRAIT_ID__UID, i+1);     // i+1 to avoid giving out a UID of zero.
        cpu.SetTrait(TRAIT_ID__OPINION, 0);   // Opinion of 0 implies that no opinion was set.
      }
    }

    ~Deme() {
      Reset();
      grid.clear();
    }

    /// Reset the deme.
    /// Reset everything! After this, deme hardware has no valid program loaded onto it.
    /// Will want to use SetProgram function before trying to run the deme.
    void Reset() {
      germ_prog.Clear();
      uids.clear();
      valid_votes.clear();
      max_vote_cnt = 0;
      for (size_t i = 0; i < grid.size(); ++i) {
        schedule[i] = i;          // Re-jigger the schedule ordering.
        uids.emplace(i+1);        // UID = grid ID + 1; +1 to avoid giving out a UID of zero.
        grid[i].ResetHardware();  // Reset CPU hardware and traits (below).
        grid[i].SetTrait(TRAIT_ID__LOC, i);
        grid[i].SetTrait(TRAIT_ID__DIR, 0);
        grid[i].SetTrait(TRAIT_ID__UID, i+1);
        grid[i].SetTrait(TRAIT_ID__OPINION, 0);
      }
    }

    /// Load a new program (_germ) onto all hardware in the deme.
    /// NOTE: This function will completely reset the deme before loading the new program onto each
    ///       hardware unit. That means you'll reset the cores, shared memories, event queues,
    ///       traits, etc. of each hardware unit in the deme. Once the new program is loaded, this
    ///       function takes care of putting all hardware units back into a valid execution state (i.e.
    ///       it'll spawn a main core post-load).
    void SetProgram(const program_t & _germ) {
      Reset();
      germ_prog = _germ;
      for (size_t i = 0; i < grid.size(); ++i) {
        grid[i].SetProgram(germ_prog);
        grid[i].SpawnCore(0, memory_t(), true);
      }
    }

    /// Hardware configuration option.
    /// Set the maximum number of cores on all hardware units in this deme.
    void SetHardwareMaxCores(size_t max_cores) {
      for (size_t i = 0; i < grid.size(); ++i) {
        grid[i].SetMaxCores(max_cores);
      }
    }

    /// Hardware configuration option.
    /// Set the maximum call depth of all hardware units in this deme.
    void SetHardwareMaxCallDepth(size_t max_depth) {
      for (size_t i = 0; i < grid.size(); ++i) {
        grid[i].SetMaxCallDepth(max_depth);
      }
    }

    /// Hardware configuration option.
    /// Set the minimum binding threshold (for Calls, event handling, etc.) for all hardware units in this deme.
    void SetHardwareMinBindThresh(double threshold) {
      for (size_t i = 0; i < grid.size(); ++i) {
        grid[i].SetMinBindThresh(threshold);
      }
    }

    const program_t & GetProgram() const { return germ_prog; }

    size_t GetWidth() const { return width; }
    size_t GetHeight() const { return height; }
    size_t GetSize() const { return grid.size(); }

    /// Get x location in deme grid given hardware loc ID.
    size_t GetLocX(size_t id) const { return id % width; }

    /// Get Y location in deme grid given hardware loc ID.
    size_t GetLocY(size_t id) const { return id / width; }

    /// Get loc ID of hardware given an x, y position.
    size_t GetID(size_t x, size_t y) const { return (y * width) + x; }

    /// Get location adjacent to ID in direction dir.
    size_t GetNeighbor(size_t id, size_t dir) const {
      int facing_x = (int)GetLocX(id);
      int facing_y = (int)GetLocY(id);
      switch(dir) {
        case DIR_UP:    facing_y = emp::Mod(facing_y + 1, (int)height); break;
        case DIR_LEFT:  facing_x = emp::Mod(facing_x - 1, (int)width);  break;
        case DIR_RIGHT: facing_x = emp::Mod(facing_x + 1, (int)width);  break;
        case DIR_DOWN:  facing_y = emp::Mod(facing_y - 1, (int)height); break;
        default:
          std::cout << "Bad direction!" << std::endl; // TODO: put an assert here.
          break;
      }
      return GetID(facing_x, facing_y);
    }

    /// Get neighbor ID faced by hardware unit specified by id.
    size_t GetFacing(size_t id) const { return GetNeighbor(id, (size_t)grid[id].GetTrait(TRAIT_ID__DIR)); }

    bool InboxFull(size_t id) const { return inboxes[id].size() >= inbox_capacity; }
    bool InboxEmpty(size_t id) const { return inboxes[id].empty(); }

    inbox_t & GetInbox(size_t id) { return inboxes[id]; }
    hardware_t & GetHardware(size_t id) { return grid[id]; }

    void DeliverToInbox(size_t id, const event_t & event) {
      while (InboxFull(id)) inboxes[id].pop_front(); // Make room for new message in inbox. Remove oldest first.
      inboxes[id].emplace_back(event);
    }

    /// Randomize unique identifiers for each agent (range of each: [MIN_UID:MAX_UID]). Function
    /// ensures uniqueness.
    void RandomizeUIDS() {
      emp_assert(MAX_UID - MIN_UID > grid.size());
      uids.clear();
      valid_votes.clear();
      max_vote_cnt = 0;
      for (size_t i = 0; i < grid.size(); ++i) {
        size_t val = rnd->GetUInt(MIN_UID, MAX_UID);
        while (emp::Has(uids, val)) { val = rnd->GetUInt(MIN_UID, MAX_UID); }
        grid[i].SetTrait(TRAIT_ID__UID, val);
        uids.emplace(val);
      }
    }

    /// Advance deme by t timesteps. For each timestep, do a single advance of deme.
    void Advance(size_t t = 1) { for (size_t i = 0; i < t; ++i) SingleAdvance(); }

    /// Advance deme by a single timestep.
    /// Hardware scheduling order is shuffled at the beginning of a timestep.
    /// valid_votes and max_vote_cnt are updated for the single timestep.
    void SingleAdvance() {
      emp::Shuffle(*rnd, schedule); // Shuffle the schedule.
      valid_votes.clear();
      max_vote_cnt = 0;
      // Distribute the CPU cycles.
      for (size_t i = 0; i < schedule.size(); ++i) {
        grid[schedule[i]].SingleProcess();
        // Has i voted for a valid agent?
        size_t vote_i = (size_t)grid[schedule[i]].GetTrait(TRAIT_ID__OPINION);
        if (emp::Has(uids, vote_i)) {
          // If so, add i's vote.
          valid_votes.emplace(vote_i);
          size_t cnt = valid_votes.count(vote_i);
          if (cnt > max_vote_cnt) max_vote_cnt = cnt;
        }
      }
    }

    /// Prints the states of all hardware in the deme along with some voting information.
    /// This function is primarily for debugging.
    void PrintState(std::ostream & os=std::cout) {
      os << "==== DEME STATE ====\n";
      os << "  Total valid votes: " << valid_votes.size() << "\n";
      os << "  Max consensus: " << max_vote_cnt << "\n";
      os << "  Votes: ";
      for (auto it = valid_votes.begin(); it != valid_votes.end(); ++it) {
        std::cout << " {vote: " << *it << ", cnt: " << valid_votes.count(*it) << "}";
      } os << "\n";
      for (size_t i = 0; i < grid.size(); ++i) {
        os << "--- Agent @ (" << GetLocX(i) << ", " << GetLocY(i) << ") ---\n";
        grid[i].PrintState(os); os << "\n";
      }
    }
  };

  using world_t = emp::World<Agent>;

protected:
  // == Configurable experiment parameters ==
  // General settings.
  bool DEBUG_MODE;    //< Are we in debug mode? NOTE: Currenly not in use.
  int RANDOM_SEED;    //< Random seed to use for this experiment.
  size_t DEME_CNT;    //< Population size. i.e. the number of demes in the population at each generation.
  size_t GENERATIONS; //< How many generations (iterations of evolution) should we run the experiment?
  std::string ANCESTOR_FPATH; //< File path to ancestor program description.
  // Hardware-specific settings.
  bool EVENT_DRIVEN;         //< Is this consensus experiment event driven?
  size_t INBOX_CAPACITY;     //< Message inbox capacity for agents. Only relevant for imperative agents.
  bool FORK_ON_MESSAGE;      //< Should we fork a new process in a hardware unit when it handles a message?
  size_t HW_MAX_CORES;       //< Max number of hardware cores. i.e. max number of simultaneous threads of execution hardware will support.
  size_t HW_MAX_CALL_DEPTH;  //< Max call depth of hardware unit.
  double HW_MIN_BIND_THRESH; //< Hardware minimum binding threshold.
  // Deme-specific settings.
  size_t DEME_WIDTH;      //< Width (in cells) of a deme. Deme size = deme width * deme height.
  size_t DEME_HEIGHT;     //< Height (in cells) of a deme. Deme size = deme width * deme height.
  size_t DEME_EVAL_TIME;  //< How long should each deme get to evaluate?
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
  size_t SYSTEMATICS_INTERVAL;    //< Interval to save summary statistics.
  size_t POP_SNAPSHOT_INTERVAL;   //< Interval to take full program snapshots of population.
  std::string DATA_DIRECTORY;     //< Directory in which to store all program output.

  emp::Ptr<emp::Random> random;   //< Random number generator. Exp class is responsible for allocation and deallocation.
  emp::Ptr<world_t> world;        //< Empirical world object. Exp class is responsible for allocation and deallocation.

  emp::Ptr<inst_lib_t> inst_lib;   //< Empirical hardware instruction library. Exp class is responsible for allocation and deallocation.
  emp::Ptr<event_lib_t> event_lib; //< Empirical hardware event library. Exp class is responsible for allocation and deallocation.

  emp::Ptr<Deme> eval_deme;  //< We'll use a single deme to serially evaluate everyone in the evolving population every generation.

  emp::vector<affinity_t> affinity_table; //< Convenient table of affinities. (primarily used in debugging)

public:
  ConsensusExp(const ConsensusConfig & config)
    : affinity_table(emp::Pow2(AFFINITY_WIDTH))
  {
    // Fill out experiment parameters with config settings!
    DEBUG_MODE = config.DEBUG_MODE();
    RANDOM_SEED = config.RANDOM_SEED();
    DEME_CNT = config.DEME_CNT();
    GENERATIONS = config.GENERATIONS();
    ANCESTOR_FPATH = config.ANCESTOR_FPATH();
    EVENT_DRIVEN = config.EVENT_DRIVEN();
    INBOX_CAPACITY = config.INBOX_CAPACITY();
    FORK_ON_MESSAGE = config.FORK_ON_MESSAGE();
    HW_MAX_CORES = config.HW_MAX_CORES();
    HW_MAX_CALL_DEPTH = config.HW_MAX_CALL_DEPTH();
    HW_MIN_BIND_THRESH = config.HW_MIN_BIND_THRESH();
    DEME_WIDTH = config.DEME_WIDTH();
    DEME_HEIGHT = config.DEME_HEIGHT();
    DEME_EVAL_TIME = config.DEME_EVAL_TIME();
    PROG_MAX_FUNC_CNT = config.PROG_MAX_FUNC_CNT();
    PROG_MAX_FUNC_LEN = config.PROG_MAX_FUNC_LEN();
    PROG_MAX_ARG_VAL = config.PROG_MAX_ARG_VAL();
    PER_BIT__AFFINITY_FLIP_RATE = config.PER_BIT__AFFINITY_FLIP_RATE();
    PER_INST__SUB_RATE = config.PER_INST__SUB_RATE();
    PER_FUNC__SLIP_RATE = config.PER_FUNC__SLIP_RATE();
    PER_FUNC__FUNC_DUP_RATE = config.PER_FUNC__FUNC_DUP_RATE();
    PER_FUNC__FUNC_DEL_RATE = config.PER_FUNC__FUNC_DEL_RATE();
    SYSTEMATICS_INTERVAL = config.SYSTEMATICS_INTERVAL();
    POP_SNAPSHOT_INTERVAL = config.POP_SNAPSHOT_INTERVAL();
    DATA_DIRECTORY = config.DATA_DIRECTORY();

    // Setup the output directory.
    mkdir(DATA_DIRECTORY.c_str(), ACCESSPERMS);
    if (DATA_DIRECTORY.back() != '/') DATA_DIRECTORY += '/';

    // Make our random number generator.
    random = emp::NewPtr<emp::Random>(RANDOM_SEED);

    // Make the world.
    world = emp::NewPtr<world_t>(random, "Consensus-World");
    world->Reset();

    // Create empty instruction/event libraries.
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();

    // Create the deme that will be used to evaluate evolving programs.
    eval_deme = emp::NewPtr<Deme>(random, DEME_WIDTH, DEME_HEIGHT, INBOX_CAPACITY, inst_lib, event_lib);
    // Do some hardware configuration in the deme.
    eval_deme->SetHardwareMinBindThresh(HW_MIN_BIND_THRESH);
    eval_deme->SetHardwareMaxCores(HW_MAX_CORES);
    eval_deme->SetHardwareMaxCallDepth(HW_MAX_CALL_DEPTH);

    // Fill out our convenient affinity table.
    for (size_t i = 0; i < affinity_table.size(); ++i) {
      affinity_table[i].SetByte(0, (uint8_t)i);
    }

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
    // Orientation-related instructions:
    inst_lib->AddInst("RotCW", Inst_RotCW, 0, "Rotate orientation clockwise (90 degrees) once.");
    inst_lib->AddInst("RotCCW", Inst_RotCCW, 0, "Rotate orientation counter-clockwise (90 degrees) once.");
    inst_lib->AddInst("RotDir", Inst_RotDir, 1, "Rotate to face direction specified by Arg1 (Arg1 mod 4)");
    inst_lib->AddInst("RandomDir", Inst_RandomDir, 1, "Local memory: Arg1 => RandomUInt([0:4)");
    inst_lib->AddInst("GetDir", Inst_GetDir, 1, "Local memory Arg1 => Current direction.");
    // Communication instructions:
    inst_lib->AddInst("SendMsgFacing", Inst_SendMsgFacing, 0, "Send output memory as message event to faced neighbor.", emp::ScopeType::BASIC, 0, {"affinity"});
    inst_lib->AddInst("BroadcastMsg", Inst_BroadcastMsg, 0, "Broadcast output memory as message event.", emp::ScopeType::BASIC, 0, {"affinity"});
    // Consensus-specific instructions:
    inst_lib->AddInst("GetUID", Inst_GetUID, 1, "LocalReg[Arg1] = Trait[UID]");
    inst_lib->AddInst("SetOpinion", Inst_SetOpinion, 1, "Trait[Opinion] = LocalReg[Arg1]");

    // Are we forking on a message or not? All that changes are the message event handlers.
    if (FORK_ON_MESSAGE) {
      event_lib->AddEvent("MessageFacing", HandleEvent_MessageForking, "Event for messaging neighbors.");
      event_lib->AddEvent("MessageBroadcast", HandleEvent_MessageForking, "Event for broadcasting a message.");
    } else {
      inst_lib->AddInst("Fork", Inst_Fork, 0, "Fork a new thread. Local memory contents of callee are loaded into forked thread's input memory.", emp::ScopeType::BASIC, 0, {"affinity"});
      event_lib->AddEvent("MessageFacing", HandleEvent_MessageNonForking, "Event for messaging neighbors.");
      event_lib->AddEvent("MessageBroadcast", HandleEvent_MessageNonForking, "Event for broadcasting a message.");
    }

    // - Setup event-driven vs. imperative differences. -
    // Primary difference: message dispatchers.
    // Imperative/procedural runs will make use of the message inbox and need the extra RetrieveMsg
    // instruction.
    if (EVENT_DRIVEN) {
      // Event-driven-specific.
      event_lib->RegisterDispatchFun("MessageFacing", [this](hardware_t & hw, const event_t & event) {
        this->EventDriven__DispatchMessageFacing(hw, event);
      });
      event_lib->RegisterDispatchFun("MessageBroadcast", [this](hardware_t & hw, const event_t & event) {
        this->EventDriven__DispatchMessageBroadcast(hw, event);
      });
    } else {
      // Imperative-specific.
      inst_lib->AddInst("RetrieveMsg", [this](hardware_t & hw, const inst_t & inst) {
        this->Inst_RetrieveMsg(hw, inst);
      }, 0, "Retrieve a message from message inbox.");
      event_lib->RegisterDispatchFun("MessageFacing", [this](hardware_t & hw, const event_t & event) {
        this->Imperative__DispatchMessageFacing(hw, event);
      });
      event_lib->RegisterDispatchFun("MessageBroadcast", [this](hardware_t & hw, const event_t & event) {
        this->Imperative__DispatchMessageBroadcast(hw, event);
      });
    }

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

    // Configure the world.
    world->SetWellMixed(true);                 // Deme germs are well-mixed. (no need for keeping track of deme-deme spatial information)
    world->SetFitFun([this](Agent & agent) { return this->CalcFitness(agent); });
    world->SetMutFun([this](Agent & agent, emp::Random & rnd) { return this->Mutate(agent, rnd); });
    world->Inject(ancestor_prog, DEME_CNT);    // Inject a bunch of ancestor deme-germs into the population.

    // Setup the systematics output file.
    auto & sys_file = world->SetupSystematicsFile(DATA_DIRECTORY + "systematics.csv");
    sys_file.SetTimingRepeat(SYSTEMATICS_INTERVAL);

  }

  ~ConsensusExp() {
    world.Delete();
    eval_deme.Delete();
    inst_lib.Delete();
    event_lib.Delete();
    random.Delete();
  }

  /// Run the experiment!
  void Run() {
    size_t full_consensus_time = 0; // These are used for printing summary information about each update to std out.
    size_t best_agent = 0;
    double best_score = 0;
    for (size_t ud = 0; ud <= GENERATIONS; ++ud) {
      // Evaluate each agent.
      best_agent = 0;
      best_score = 0;
      for (size_t id = 0; id < world->GetSize(); ++id) {
        eval_deme->SetProgram(world->GetGenomeAt(id));  // Load agent's program onto evaluation deme.
        eval_deme->RandomizeUIDS();         // Randomize evaluation deme hardwares' UIDs.
        full_consensus_time = 0;            // Reset full consensus tracker.
        // Run the deme for some amount of time.
        for (size_t t = 0; t < DEME_EVAL_TIME; ++t) {
          // std::cout << "=============================== TIME: " << t << " ===============================" << std::endl;
          eval_deme->SingleAdvance();
          // Was there consensus?
          if (eval_deme->max_vote_cnt == eval_deme->GetSize()) ++full_consensus_time;
        }
        // Compute some relevant information about deme performance.
        Agent & agent = world->GetOrg(id);
        agent.max_consensus = eval_deme->max_vote_cnt;  // max vote count will have max consensus size at end of evaluation.
        agent.valid_votes = eval_deme->valid_votes.size(); // TODO: check to make sure this is what we're expecting.
        agent.full_consensus_time = full_consensus_time;
        double score = CalcFitness(agent);
        if (score > best_score) { best_score = score; best_agent = id; }
      }

      // Selection (nothing particularly interesting here).
      // Keep the best program around.
      emp::EliteSelect(*world, 1, 1);
      // Run a tournament for the rest.
      emp::TournamentSelect(*world, 8, DEME_CNT - 1);

      // Print out in-run summary stats on dominant agent from last generation (which will be the first one).
      std::cout << "Update " << world->GetUpdate();
      std::cout << ", Max score: " << best_score << std::endl;
      std::cout << "    Final max consensus: " << world->GetOrg(best_agent).max_consensus << std::endl;
      std::cout << "    Final valid votes: " << world->GetOrg(best_agent).valid_votes << std::endl;
      std::cout << "    Time at consensus: " << world->GetOrg(best_agent).full_consensus_time << std::endl;

      // Update the world (generational turnover).
      world->Update();

      // Mutate everyone but the first (elite) agent.
      world->DoMutations(1);

      // Snapshot time?
      if (ud % POP_SNAPSHOT_INTERVAL == 0) Snapshot(ud);
    }
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

  /// Compute fitness of given agent. Assumes that agent has been evaluated already and that its
  /// full_consensus_time, valid_votes, max_consensus member variables have been appropriately filled out.
  /// Agent fitness = valid votes at end of evaluation + max consensus at end of evaluation +
  ///                 (number of deme updates where a full, valid consensus was maintained * deme size)
  double CalcFitness(Agent & agent) {
    return (double)(agent.valid_votes + agent.max_consensus + (agent.full_consensus_time * eval_deme->GetSize()));
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

  /// Dispatch straight to faced neighbor.
  /// NOTE: needs access to eval_deme to know who neighbors are.
  void EventDriven__DispatchMessageFacing(hardware_t hw, const event_t & event) {
    const size_t facing_id = eval_deme->GetFacing((size_t)hw.GetTrait(TRAIT_ID__LOC));
    hardware_t & rHW = eval_deme->GetHardware(facing_id);
    rHW.QueueEvent(event);
  }

  /// Dispatch to faced neighbor's inbox.
  /// NOTE: needs access to eval_deme to know who neighbors are.
  void Imperative__DispatchMessageFacing(hardware_t hw, const event_t & event) {
    const size_t facing_id = eval_deme->GetFacing((size_t)hw.GetTrait(TRAIT_ID__LOC));
    eval_deme->DeliverToInbox(facing_id, event);
  }

  /// Dispatch to all of hw's neighbors.
  /// NOTE: needs access to eval_deme to know who neighbors are.
  void EventDriven__DispatchMessageBroadcast(hardware_t hw, const event_t & event) {
    const size_t loc_id = (size_t)hw.GetTrait(TRAIT_ID__LOC);
    eval_deme->GetHardware(eval_deme->GetNeighbor(loc_id, DIR_UP)).QueueEvent(event);
    eval_deme->GetHardware(eval_deme->GetNeighbor(loc_id, DIR_DOWN)).QueueEvent(event);
    eval_deme->GetHardware(eval_deme->GetNeighbor(loc_id, DIR_RIGHT)).QueueEvent(event);
    eval_deme->GetHardware(eval_deme->GetNeighbor(loc_id, DIR_LEFT)).QueueEvent(event);
  }

  /// Dispatch to all neighbors' inboxes.
  /// NOTE: needs access to eval_deme to know who neighbors are.
  void Imperative__DispatchMessageBroadcast(hardware_t hw, const event_t & event) {
    const size_t loc_id = (size_t)hw.GetTrait(TRAIT_ID__LOC);
    eval_deme->DeliverToInbox(eval_deme->GetNeighbor(loc_id, DIR_UP), event);
    eval_deme->DeliverToInbox(eval_deme->GetNeighbor(loc_id, DIR_DOWN), event);
    eval_deme->DeliverToInbox(eval_deme->GetNeighbor(loc_id, DIR_RIGHT), event);
    eval_deme->DeliverToInbox(eval_deme->GetNeighbor(loc_id, DIR_LEFT), event);
  }

  // ============== Some instructions used in this experiment: ==============
  /// Instruction: RandomDir
  /// Description: Local[Arg1] = RandomInt(0, NUM_DIRECTIONS)
  static void Inst_RandomDir(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    state.SetLocal(inst.args[0], hw.GetRandom().GetUInt(0, NUM_NEIGHBORS));
  }

  /// Instruction: RotCW
  /// Description: Rotate clockwise once.
  static void Inst_RotCW(hardware_t & hw, const inst_t & inst) {
    hw.SetTrait(TRAIT_ID__DIR, emp::Mod(hw.GetTrait(TRAIT_ID__DIR) - 1, NUM_NEIGHBORS));
  }

  /// Instruction: RotCCW
  /// Description: Rotate counter-clockwise once.
  static void Inst_RotCCW(hardware_t & hw, const inst_t & inst) {
    hw.SetTrait(TRAIT_ID__DIR, emp::Mod(hw.GetTrait(TRAIT_ID__DIR) + 1, NUM_NEIGHBORS));
  }

  /// Instruction: RotDir
  /// Description: Rotate to face direction specified by Local[Arg1] % NUM_NEIGHBORS.
  static void Inst_RotDir(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    hw.SetTrait(TRAIT_ID__DIR, emp::Mod((int)state.AccessLocal(inst.args[0]), NUM_NEIGHBORS));
  }

  /// Instruction: GetDir
  /// Description: Local[Arg1] = Current direction.
  static void Inst_GetDir(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    state.SetLocal(inst.args[0], hw.GetTrait(TRAIT_ID__DIR));
  }

  /// Instruction: SendMsgFacing
  /// Description: Send message to faced neighbor (as determined by hardware direction trait).
  static void Inst_SendMsgFacing(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    hw.TriggerEvent("MessageFacing", inst.affinity, state.output_mem, {"send"});
  }

  /// Instruction: BroadcastMsg
  /// Description: Broadcast a message to all neighbors.
  static void Inst_BroadcastMsg(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    hw.TriggerEvent("MessageBroadcast", inst.affinity, state.output_mem, {"broadcast"});
  }

  /// Instruction: GetUID
  /// Description: Local[Arg1] = Trait[UID]
  static void Inst_GetUID(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    state.SetLocal(inst.args[0], hw.GetTrait(TRAIT_ID__UID));
  }

  /// Instruction: SetOpinion
  /// Description: Trait[Opinion] = Local[Arg1]
  static void Inst_SetOpinion(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    double val = state.AccessLocal(inst.args[0]);
    if (val > 0) hw.SetTrait(TRAIT_ID__OPINION, (int)val);
  }

  /// Instruction: Fork
  /// Description: Fork thread with local memory as new thread's input buffer.
  static void Inst_Fork(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    hw.SpawnCore(inst.affinity, hw.GetMinBindThresh(), state.local_mem);
  }

  /// Instruction: RetrieveMsg
  /// Description: Retrieve a message from the hardware's associated message inbox if there is a message
  ///              to be retrieved. Used in procedural representation runs.
  /// NOTE: needs access to eval_deme for message inboxes.
  void Inst_RetrieveMsg(hardware_t hw, const inst_t & inst) {
    const size_t loc_id = (size_t)hw.GetTrait(TRAIT_ID__LOC);
    if (!eval_deme->InboxEmpty(loc_id)) {
      hw.HandleEvent(eval_deme->GetInbox(loc_id).front());  // NOTE: Assumes that Event handler won't mess with inbox.
      eval_deme->GetInbox(loc_id).pop_front();
    }
  }

  // ============== Some event handlers used in this experiment: ==============

  /// Event handler: MessageForking
  /// Description: Handles a message by spawning a new core with event data.
  static void HandleEvent_MessageForking(hardware_t & hw, const event_t & event) {
    // Spawn a new core.
    hw.SpawnCore(event.affinity, hw.GetMinBindThresh(), event.msg);
  }

  /// Event handler: MessageNonForking
  /// Description: Handles a message by loading event contents into local memory buffer.
  static void HandleEvent_MessageNonForking(hardware_t & hw, const event_t & event) {
    // Instead of spawning a new core. Load event data into input buffer of current call state.
    state_t & state = hw.GetCurState();
    // Loop through event memory....
    for (auto mem : event.msg) { state.SetInput(mem.first, mem.second); }
  }

};

int main(int argc, char * argv[])
{
  // Read configs.
  std::string config_fname = "configs.cfg";
  auto args = emp::cl::ArgManager(argc, argv);
  ConsensusConfig config;
  config.Read(config_fname);
  if (args.ProcessConfigOptions(config, std::cout, config_fname, "consensus-config.h") == false) exit(0);
  if (args.TestUnknown() == false) exit(0);

  std::cout << "==============================" << std::endl;
  std::cout << "|    How am I configured?    |" << std::endl;
  std::cout << "==============================" << std::endl;
  config.Write(std::cout);
  std::cout << "==============================\n" << std::endl;

  // Create experiment with configs, then run it!
  ConsensusExp e(config);
  e.Run();
}
