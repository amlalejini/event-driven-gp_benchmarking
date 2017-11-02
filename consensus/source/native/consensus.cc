// This is the main function for the NATIVE version of this project.

#include <iostream>
#include <deque>
#include <unordered_set>

#include "base/Ptr.h"
#include "base/vector.h"
#include "config/ArgManager.h"
#include "tools/Random.h"
#include "tools/random_utils.h"
#include "tools/math.h"
#include "hardware/EventDrivenGP.h"
#include "Evo/World.h"

#include "consensus-config.h"

constexpr size_t AFFINITY_WIDTH = 16;

constexpr size_t TRAIT_ID__LOC     = 0;
constexpr size_t TRAIT_ID__DIR     = 1;
constexpr size_t TRAIT_ID__UID     = 2;
constexpr size_t TRAIT_ID__OPINION = 3;

constexpr size_t NUM_NEIGHBORS = 4;

constexpr size_t DIR_UP = 0;
constexpr size_t DIR_LEFT = 1;
constexpr size_t DIR_DOWN = 2;
constexpr size_t DIR_RIGHT = 3;

constexpr size_t MIN_UID = 1;
constexpr size_t MAX_UID = 10000;

// class ImperativeGP : public emp::EventDrivenGP {
// public:
//   using emp::EventDrivenGP::event_t;
// protected:
//   std::deque<event_t> msg_inbox;
//   size_t msg_inbox_capacity;
// public:
//   bool InboxFull() { return msg_inbox.size() >= msg_inbox_capacity; }
//   // NOTE: This will not work.
//   /// Instruction to retrieve a message from the message inbox.
//   static void RetrieveMsg(ImperativeGP & hw, const inst_t inst) {
//     std::cout << "Retrieve a message from the message inbox!" << std::endl;
//     if (!hw.msg_inbox.empty()) {
//       const event_t & msg = hw.msg_inbox.front();
//       hw.SpawnCore(msg.affinity, hw.GetMinBindThresh(), msg.msg);
//       hw.msg_inbox.pop_front();
//     }
//   }
//
//   /// Event handler to
//   static void HandleEvent_Message(ImperativeGP & hw, const event_t & event) {
//     std::cout << "Handling message event!" << std::endl;
//     while (hw.InboxFull()) hw.msg_inbox.pop_front(); // Make room for new message in inbox. Remove oldest first.
//     hw.msg_inbox.emplace_back(event);
//   }
// };

/// Class to manage a concensus experiment.
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
  struct Agent {
    program_t program;

    Agent(const program_t & _p)
      : program(_p) { ; }

    program_t & GetGenome() { return program; }
  };

  /// Struct to keep track of demes for the Consensus experiment.
  struct Deme {
    using grid_t = emp::vector<hardware_t>;
    using inbox_t = std::deque<event_t>;

    grid_t grid;
    size_t width;
    size_t height;
    size_t inbox_capacity;

    emp::vector<size_t> schedule;
    emp::vector<inbox_t> inboxes;

    std::unordered_set<size_t> uids;

    emp::Ptr<emp::Random> rnd;
    emp::Ptr<Agent> germ;

    Deme(emp::Ptr<emp::Random> _rnd, size_t _w, size_t _h, size_t _ibox_cap, emp::Ptr<inst_lib_t> _ilib, emp::Ptr<event_lib_t> _elib)
      : grid(), width(_w), height(_h), inbox_capacity(_ibox_cap),
        schedule(width*height), inboxes(width*height),
        rnd(_rnd), germ(nullptr)
    {
      // Fill out the grid with hardware.
      for (size_t i = 0; i < width * height; ++i) {
        grid.emplace_back(_ilib, _elib, rnd);
        schedule[i] = i;
        uids.emplace(i);
        hardware_t & cpu = grid.back();
        cpu.SetTrait(TRAIT_ID__LOC, i);
        cpu.SetTrait(TRAIT_ID__DIR, 0);
        cpu.SetTrait(TRAIT_ID__UID, i);
        cpu.SetTrait(TRAIT_ID__OPINION, 0); // Opinion of 0 implies that no opinion was set.
        // Configure CPU
        // cpu.SetMinBind();
        //
      }
    }

    ~Deme() {
      Reset();
      grid.clear();
    }

    void Reset() {
      germ = nullptr;
      // TODO: reset traits!
      for (size_t i = 0; i < grid.size(); ++i) { grid[i].ResetHardware(); }
    }

    void SetGerm(emp::Ptr<Agent> _germ) {
      Reset();
      germ = _germ;
      for (size_t i = 0; i < grid.size(); ++i) {
        grid[i].SetProgram(germ->program);
        grid[i].SpawnCore(0, memory_t(), true);
      }
    }

    size_t GetWidth() const { return width; }
    size_t GetHeight() const { return height; }

    size_t GetLocX(size_t id) const { return id % width; }
    size_t GetLocY(size_t id) const { return id / width; }
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

    size_t GetFacing(size_t id) const { return GetNeighbor(id, (size_t)grid[id].GetTrait(TRAIT_ID__DIR)); }

    bool InboxFull(size_t id) const { return inboxes[id].size() >= inbox_capacity; }
    bool InboxEmpty(size_t id) const { return inboxes[id].empty(); }

    inbox_t & GetInbox(size_t id) { return inboxes[id]; }
    hardware_t & GetHardware(size_t id) { return grid[id]; }

    void DeliverToInbox(size_t id, const event_t & event) {
      while (InboxFull(id)) inboxes[id].pop_front(); // Make room for new message in inbox. Remove oldest first.
      inboxes[id].emplace_back(event);
    }

    /// Randomize unique identifiers for each agent.
    void RandomizeUIDS() {
      emp_assert(MAX_UID - MIN_UID > grid.size());
      uids.clear();
      for (size_t i = 0; i < grid.size(); ++i) {
        size_t val = rnd->GetUInt(MIN_UID, MAX_UID);
        while (emp::Has(uids, val)) { val = rnd->GetUInt(MIN_UID, MAX_UID); }
        grid[i].SetTrait(TRAIT_ID__UID, val);
      }
    }

    void Advance(size_t t = 1) { for (size_t i = 0; i < t; ++i) SingleAdvance(); }

    void SingleAdvance() {
      emp::Shuffle(*rnd, schedule); // Shuffle the schedule.
      // Distribute the CPU cycles.
      for (size_t i = 0; i < schedule.size(); ++i) { grid[schedule[i]].SingleProcess(); }
    }
  };

  using world_t = emp::World<Agent>;

protected:
  // == Configurable experiment parameters ==
  // TODO: PARAMETERIZE THIS STUFF! (using config system)
  int RANDOM_SEED;
  bool EVENT_DRIVEN;
  size_t DEME_WIDTH;
  size_t DEME_HEIGHT;
  size_t INBOX_CAPACITY;

  emp::Ptr<emp::Random> random;
  emp::Ptr<world_t> world;

  emp::Ptr<inst_lib_t> inst_lib;
  emp::Ptr<event_lib_t> event_lib;

  emp::Ptr<Deme> eval_deme;

  emp::vector<affinity_t> affinity_table;

public:
  ConsensusExp(int _random_seed, bool _event_driven)
    : RANDOM_SEED(_random_seed), EVENT_DRIVEN(_event_driven),
      DEME_WIDTH(5), DEME_HEIGHT(5), INBOX_CAPACITY(8),
      affinity_table(emp::Pow2(AFFINITY_WIDTH))
  {
    random = emp::NewPtr<emp::Random>(RANDOM_SEED);
    world = emp::NewPtr<world_t>(random, "Consensus-World");
    inst_lib = emp::NewPtr<inst_lib_t>();
    event_lib = emp::NewPtr<event_lib_t>();
    eval_deme = emp::NewPtr<Deme>(random, DEME_WIDTH, DEME_HEIGHT, INBOX_CAPACITY, inst_lib, event_lib);

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
    inst_lib->AddInst("GetUID", Inst_GetUID, 1, "");
    inst_lib->AddInst("SetOpinion", Inst_SetOpinion, 1, "");

    // - Setup event library. -
    event_lib->AddEvent("MessageFacing", HandleEvent_MessageForking, "Event for messaging neighbors.");
    event_lib->AddEvent("MessageBroadcast", HandleEvent_MessageForking, "Event for broadcasting a message.");

    // - Setup event-driven vs. imperative differences. -
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
        this->EventDriven__DispatchMessageBroadcast(hw, event);
      });
    }

    // TODO: Non-forking message handler?
    world->SetWellMixed(true);

  }

  ~ConsensusExp() {
    world.Delete();
    random.Delete();
  }

  /// Dispatch straight to neighbor.
  void EventDriven__DispatchMessageFacing(hardware_t hw, const event_t & event) {
    const size_t facing_id = eval_deme->GetFacing((size_t)hw.GetTrait(TRAIT_ID__LOC));
    hardware_t & rHW = eval_deme->GetHardware(facing_id);
    rHW.QueueEvent(event);
  }

  /// Dispatch to neighbor's inbox.
  void Imperative__DispatchMessageFacing(hardware_t hw, const event_t & event) {
    const size_t facing_id = eval_deme->GetFacing((size_t)hw.GetTrait(TRAIT_ID__LOC));
    eval_deme->DeliverToInbox(facing_id, event);
  }

  /// Dispatch to all neighbors.
  void EventDriven__DispatchMessageBroadcast(hardware_t hw, const event_t & event) {
    const size_t loc_id = (size_t)hw.GetTrait(TRAIT_ID__LOC);
    eval_deme->GetHardware(eval_deme->GetNeighbor(loc_id, DIR_UP)).QueueEvent(event);
    eval_deme->GetHardware(eval_deme->GetNeighbor(loc_id, DIR_DOWN)).QueueEvent(event);
    eval_deme->GetHardware(eval_deme->GetNeighbor(loc_id, DIR_RIGHT)).QueueEvent(event);
    eval_deme->GetHardware(eval_deme->GetNeighbor(loc_id, DIR_LEFT)).QueueEvent(event);
  }

  /// Dispatch to all neighbors' inbox.
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
  /// Description:
  static void Inst_BroadcastMsg(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    hw.TriggerEvent("MessageBroadcast", inst.affinity, state.output_mem, {"broadcast"});
  }

  /// Instruction:
  /// Description:
  static void Inst_GetUID(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    state.SetLocal(inst.args[0], hw.GetTrait(TRAIT_ID__UID));
  }

  /// Instruction:
  /// Description:
  static void Inst_SetOpinion(hardware_t & hw, const inst_t & inst) {
    state_t & state = hw.GetCurState();
    hw.SetTrait(TRAIT_ID__OPINION, state.AccessLocal(inst.args[0]));
  }

  /// Instruction: RetrieveMsg
  /// Description:
  void Inst_RetrieveMsg(hardware_t hw, const inst_t & inst) {
    const size_t loc_id = (size_t)hw.GetTrait(TRAIT_ID__LOC);
    if (!eval_deme->InboxEmpty(loc_id)) {
      hw.HandleEvent(eval_deme->GetInbox(loc_id).front());  // NOTE: Assumes that Event handler won't mess with inbox.
      eval_deme->GetInbox(loc_id).pop_front();
    }
  }

  // ============== Some event handlers used in this experiment: ==============
  static void HandleEvent_MessageForking(hardware_t & hw, const event_t event) {
    // Spawn a new core.
    hw.SpawnCore(event.affinity, hw.GetMinBindThresh(), event.msg);
  }

  // TODO: non-forking version of handle event?

};

int main(int argc, char * argv[])
{
  // TODO: Create/configure consensus experiment.
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
  std::cout << "==============================" << std::endl;

  int RANDOM_SEED        = config.RANDOM_SEED();
  bool EVENT_DRIVEN      = true;
  size_t DEME_WIDTH      = 5;
  size_t DEME_HEIGHT     = 5;
  size_t INBOX_CAPACITY  = 8;

  ConsensusExp e(RANDOM_SEED, EVENT_DRIVEN);
  std::cout << "Hello there!" << std::endl;
}
