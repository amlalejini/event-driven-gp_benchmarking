// This is the main function for the NATIVE version of this project.

#include <iostream>
#include <string>

#include "base/vector.h"
#include "base/Ptr.h"
#include "tools/Random.h"
#include "config/command_line.h"
#include "config/ArgManager.h"
#include "hardware/EventDrivenGP.h"

// Globals!
constexpr size_t TAG_WIDTH = 16;

constexpr size_t SIMILARITY_ADJUSTMENT_METHOD_ID__MULT = 0;
constexpr size_t SIMILARITY_ADJUSTMENT_METHOD_ID__ADD = 1;

// Aliases! 
using hardware_t = emp::EventDrivenGP_AW<TAG_WIDTH>;
using program_t = hardware_t::Program;
using state_t = hardware_t::State;
using function_t = hardware_t::Function;
using inst_t = hardware_t::inst_t;
using inst_lib_t = hardware_t::inst_lib_t;
using event_t = hardware_t::event_t;
using event_lib_t = hardware_t::event_lib_t;
using memory_t = hardware_t::memory_t;
using tag_t = hardware_t::affinity_t;
using exec_stk_t = hardware_t::exec_stk_t;

using inst_func_t = std::function<void(hardware_t &, const inst_t &)>;

// Setup configuration options
EMP_BUILD_CONFIG( FuncRegConfig,
  GROUP(DEFAULT, "Default settings for box experiment"),
  VALUE(SEED, int, 0, "Random number seed (0 for based on time)"),
  VALUE(EVAL_TIME, size_t, 16, "Time to evaluate test program"),
  VALUE(TEST_PROGRAM, std::string, "test.gp", "Test program file"),
  GROUP(REFERENCE_MODIFIER, "Reference modification group."),
  VALUE(SIMILARITY_ADJUSTMENT_METHOD, size_t, 0, "How should regulation be applied? \n0: Multiplicative\n1: Additive"),
  VALUE(REGULATORY_ADJUSTMENT, double, 0.1, "What value do we use when adjusting reference modifier?")
)

// Instruction definitions
void Inst_Fork(hardware_t & hw, const inst_t & inst) {
  state_t & state = hw.GetCurState();
  hw.SpawnCore(inst.affinity, hw.GetMinBindThresh(), state.local_mem, false, true);
}

void Inst_Terminate(hardware_t & hw, const inst_t & inst)  {
  // Pop all the call states from current core.
  exec_stk_t & core = hw.GetCurCore();
  core.resize(0);
}

void Inst_Call(hardware_t & hw, const inst_t & inst) {
  hw.CallFunction(inst.affinity, hw.GetMinBindThresh(), true);
}

int main(int argc, char* argv[])
{
  // Read configs.
  std::string config_fname = "configs.cfg";
  auto args = emp::cl::ArgManager(argc, argv);
  FuncRegConfig config;
  config.Read(config_fname);

  if (args.ProcessConfigOptions(config, std::cout, config_fname, "FuncReg-config.h") == false)
    exit(0);
  if (args.TestUnknown() == false)
    exit(0);

  std::cout << "==============================" << std::endl;
  std::cout << "|    How am I configured?    |" << std::endl;
  std::cout << "==============================" << std::endl;
  config.Write(std::cout);
  std::cout << "==============================\n"
            << std::endl;


  // Localize configs. 
  int RANDOM_SEED = config.SEED();
  size_t SIMILARITY_ADJUSTMENT_METHOD = config.SIMILARITY_ADJUSTMENT_METHOD();
  double REGULATORY_ADJUSTMENT = config.REGULATORY_ADJUSTMENT();
  size_t EVAL_TIME = config.EVAL_TIME();
  std::string TEST_PROGRAM = config.TEST_PROGRAM();

  // Some config-dependent variable declarations. 
  inst_func_t inst__up_reg_fun;
  inst_func_t inst__down_reg_fun;

  std::function<void(double, const function_t &)> apply_ref_mod_fun;

  // Setup random number generator.
  emp::Ptr<emp::Random> random = emp::NewPtr<emp::Random>(RANDOM_SEED);

  // Setup hardware. 
  emp::Ptr<inst_lib_t> inst_lib = emp::NewPtr<inst_lib_t>();
  emp::Ptr<event_lib_t> event_lib = emp::NewPtr<event_lib_t>();
  
  // TODO: Try out different ways of adjusting things! (currently, just add or subtract constant value)
  inst__up_reg_fun = [&random, &REGULATORY_ADJUSTMENT](hardware_t & hw, const inst_t & inst) {
    // What function should we up regulate? 
    // - Use instruction's tag, don't worry about minimum binding threshold, don't use reference modifier to find target.
    emp::vector<size_t> targets(hw.FindBestFuncMatch(inst.affinity, 0.0, false));
    size_t targetID;
    if (targets.empty()) return; 
    if (targets.size() == 1) targetID = targets[0];
    else targetID = targets[random->GetUInt(targets.size())];
    program_t & program = hw.GetProgram();
    double cur_mod = program[targetID].GetRefModifier();
    program[targetID].SetRefModifier(cur_mod + REGULATORY_ADJUSTMENT);
  };

  inst__down_reg_fun = [&random, &REGULATORY_ADJUSTMENT](hardware_t & hw, const inst_t & inst) {
    emp::vector<size_t> targets(hw.FindBestFuncMatch(inst.affinity, 0.0, false));
    size_t targetID;
    if (targets.empty()) return; 
    if (targets.size() == 1) targetID = targets[0];
    else targetID = targets[random->GetUInt(targets.size())];
    program_t & program = hw.GetProgram();
    double cur_mod = program[targetID].GetRefModifier();
    program[targetID].SetRefModifier(cur_mod - REGULATORY_ADJUSTMENT);
  };

  // Configure instruction library!
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
  inst_lib->AddInst("Return", hardware_t::Inst_Return, 0, "Return from current function if possible.");
  inst_lib->AddInst("SetMem", hardware_t::Inst_SetMem, 2, "Local memory: Arg1 = numerical value of Arg2");
  inst_lib->AddInst("CopyMem", hardware_t::Inst_CopyMem, 2, "Local memory: Arg1 = Arg2");
  inst_lib->AddInst("SwapMem", hardware_t::Inst_SwapMem, 2, "Local memory: Swap values of Arg1 and Arg2.");
  inst_lib->AddInst("Input", hardware_t::Inst_Input, 2, "Input memory Arg1 => Local memory Arg2.");
  inst_lib->AddInst("Output", hardware_t::Inst_Output, 2, "Local memory Arg1 => Output memory Arg2.");
  inst_lib->AddInst("Commit", hardware_t::Inst_Commit, 2, "Local memory Arg1 => Shared memory Arg2.");
  inst_lib->AddInst("Pull", hardware_t::Inst_Pull, 2, "Shared memory Arg1 => Shared memory Arg2.");
  inst_lib->AddInst("Nop", hardware_t::Inst_Nop, 0, "No operation.");
  inst_lib->AddInst("Terminate", Inst_Terminate, 0, "Kill current thread.");

  inst_lib->AddInst("Fork", Inst_Fork, 0, "Fork a new thread. Local memory contents of callee are loaded into forked thread's input memory.");
  inst_lib->AddInst("Call", Inst_Call, 0, "Call function that best matches call affinity.", emp::ScopeType::BASIC, 0, {"affinity"});
  
  // Add regulatory functions. 
  inst_lib->AddInst("UpRegulate", [&inst__up_reg_fun](hardware_t & hw, const inst_t & inst) {
    inst__up_reg_fun(hw, inst);
  }, 0, "Up regulate!");

  inst_lib->AddInst("DownRegulate", [&inst__down_reg_fun](hardware_t & hw, const inst_t & inst) {
    inst__down_reg_fun(hw, inst);
  }, 0, "Down regulate!");

  // Setup the hardware!
  emp::Ptr<hardware_t> eval_hw = emp::NewPtr<hardware_t>(inst_lib, event_lib, random);

  // When we apply regulation to a similarity measure, how do we do it? Multiplicative, additive, etc?
  switch (SIMILARITY_ADJUSTMENT_METHOD) {
    case SIMILARITY_ADJUSTMENT_METHOD_ID__MULT: {
      eval_hw->SetBaseFuncRefMod(1.0);
      eval_hw->SetFuncRefModifier([](double base_sim, const function_t & function) {
        return base_sim * function.GetRefModifier();
      });
      break;
    }
    case SIMILARITY_ADJUSTMENT_METHOD_ID__ADD: {
      eval_hw->SetBaseFuncRefMod(0.0);
      eval_hw->SetFuncRefModifier([](double base_sim, const function_t & function) {
        return base_sim + function.GetRefModifier();
      });
      break;
    }
    default: {
      std::cout << "Unrecognized similarity adjustment method! Exiting..." << std::endl;
      exit(-1);
    }
  }

  // 1) Load test program!
  std::cout << "Loading test program!" << std::endl;
  program_t test_prog(inst_lib);
  std::ifstream test_prog_fstream(TEST_PROGRAM);
  if (!test_prog_fstream.is_open()) {
    std::cout << "Failed to open test program file (" << TEST_PROGRAM << ")!" << std::endl;
    exit(-1);
  }
  test_prog.Load(test_prog_fstream);
  std::cout << " --- Test program: --- " << std::endl;
  test_prog.PrintProgramFull();
  std::cout << " --------------------- " << std::endl;

  // 2) Run program!
  eval_hw->SetProgram(test_prog);
  eval_hw->ResetHardware();
  eval_hw->SpawnCore(0, memory_t(), false);
  // - Print hardware state
  std::cout << "=== INITIAL STATE ===" << std::endl;
  eval_hw->PrintState();
  for (size_t t = 0; t < EVAL_TIME; ++t) {
    eval_hw->SingleProcess();
    std::cout << "=== T: " << t << " ===" << std::endl;
    // Print function modifiers
    std::cout << "Function modifiers:";
    for (size_t fID = 0; fID < eval_hw->GetProgram().GetSize(); ++fID) {
      std::cout << " " << fID << ":" << eval_hw->GetProgram()[fID].GetRefModifier(); 
    } std::cout << "\n";
    eval_hw->PrintState();
  }
  
  // 3) ???

  // Cleanup!
  eval_hw.Delete();
  inst_lib.Delete();
  event_lib.Delete();
  random.Delete();
}
