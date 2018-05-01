#ifndef TMAZE_CONFIG_H
#define TMAZE_CONFIG_H

#include "config/config.h"

EMP_BUILD_CONFIG( TMazeConfig,
  GROUP(DEFAULT_GROUP, "General Settings"),
  VALUE(RUN_MODE, size_t, 0, "What mode are we running in? \n0: Native experiment\n1: Analyze mode"),
  VALUE(RANDOM_SEED, int, -1, "Random number seed (negative value for based on time)"),
  VALUE(POP_SIZE, size_t, 1000, "Total population size"),
  VALUE(GENERATIONS, size_t, 100, "How many generations should we run evolution?"),
  VALUE(ANCESTOR_FPATH, std::string, "ancestor.gp", "Ancestor program file"),
  VALUE(EVALUATION_CNT, size_t, 1, "How many fitness evaluations to do per agent fitness calculation?"),
  GROUP(SELECTION_GROUP, "Selection Settings"),
  VALUE(TOURNAMENT_SIZE, size_t, 4, "How big are tournaments when using tournament selection or any selection method that uses tournaments?"),
  VALUE(SELECTION_METHOD, size_t, 0, "Which selection method are we using? \n0: Tournament\n1: Lexicase\n2: Eco-EA (resource)\n3: MAP-Elites\n4: Roulette"),
  VALUE(ELITE_SELECT__ELITE_CNT, size_t, 1, "How many elites get free reproduction passes?"),
  GROUP(SGP_FUNCTION_REGULATION_GROUP, "SignalGP function regulation settings"),
  VALUE(SIMILARITY_ADJUSTMENT_METHOD, size_t, 0, "How should regulation be applied? \n0: Additive \n1: Multiplicative "),
  VALUE(REF_MOD_ADJUSTMENT_TYPE, size_t, 0, "How should reference modifiers be adjusted on up/down regulate instruction execution? \n0: Additive \n1: Multiplicative \n2: Power \n3: Binary on/off"),
  VALUE(REF_MOD_ADJUSTMENT_VALUE, double, 0.1, "What value do we use when adjusting reference modifier?"),
  VALUE(MODIFY_REG, bool, false, "Does the reference modifier alter up/down reg similarity calculations? "),
  GROUP(SGP_PROGRAM_GROUP, "SignalGP program Settings"),
  VALUE(SGP_PROG_MAX_FUNC_CNT, size_t, 8, "Used for generating SGP programs. How many functions do we generate?"),
  VALUE(SGP_PROG_MIN_FUNC_CNT, size_t, 1, "Used for generating SGP programs. How many functions do we generate?"),
  VALUE(SGP_PROG_MAX_FUNC_LEN, size_t, 8, "Maximum number of instructions allowed per-function."),
  VALUE(SGP_PROG_MIN_FUNC_LEN, size_t, 1, "Minimum number of instructions allowed per-function."),
  VALUE(SGP_PROG_MAX_TOTAL_LEN, size_t, 256, "Maximum length of SGP programs."),
  GROUP(SGP_HARDWARE_GROUP, "SignalGP Hardware Settings"),
  VALUE(SGP_HW_MAX_CORES, size_t, 16, "Max number of hardware cores; i.e., max number of simultaneous threads of execution hardware will support."),
  VALUE(SGP_HW_MAX_CALL_DEPTH, size_t, 128, "Max call depth of hardware unit"),
  VALUE(SGP_HW_MIN_BIND_THRESH, double, 0.0, "Hardware minimum referencing threshold"),
  GROUP(SGP_MUTATION_GROUP, "SignalGP Mutation Settings"),
  VALUE(SGP__PROG_MAX_ARG_VAL, int, 16, "Maximum argument value for instructions."),
  VALUE(SGP__PER_BIT__TAG_BFLIP_RATE, double, 0.005, "Per-bit mutation rate of tag bit flips."),
  VALUE(SGP__PER_INST__SUB_RATE, double, 0.005, "Per-instruction/argument subsitution rate."),
  VALUE(SGP__PER_INST__INS_RATE, double, 0.005, "Per-instruction insertion mutation rate."),
  VALUE(SGP__PER_INST__DEL_RATE, double, 0.005, "Per-instruction deletion mutation rate."),
  VALUE(SGP__PER_FUNC__SLIP_RATE, double, 0.05, "Per-function rate of slip mutations."),
  VALUE(SGP__PER_FUNC__FUNC_DUP_RATE, double, 0.05, "Per-function rate of function duplications."),
  VALUE(SGP__PER_FUNC__FUNC_DEL_RATE, double, 0.05, "Per-function rate of function deletions."),
  GROUP(DATA_GROUP, "Data Collection Settings"),
  VALUE(SYSTEMATICS_INTERVAL, size_t, 100, "Interval to record systematics summary stats."),
  VALUE(FITNESS_INTERVAL, size_t, 100, "Interval to record fitness summary stats."),
  VALUE(POP_SNAPSHOT_INTERVAL, size_t, 10000, "Interval to take a full snapshot of the population."),
  VALUE(DATA_DIRECTORY, std::string, "./", "Location to dump data output.")
)

#endif
