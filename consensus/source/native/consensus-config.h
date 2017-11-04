#ifndef CONSENSUS_CONFIG_H
#define CONSENSUS_CONFIG_H

#include "config/config.h"

EMP_BUILD_CONFIG( ConsensusConfig,
  GROUP(DEFAULT_GROUP, "General Settings"),
  VALUE(DEBUG_MODE, bool, false, "Should we output debug information?"),
  VALUE(RANDOM_SEED, int, 2, "Random number seed (0 for based on time)"),
  VALUE(DEME_CNT, size_t, 100, "Number of demes in the population at any one time."),
  VALUE(GENERATIONS, size_t, 100, "How many generations should we run evolution?"),
  VALUE(ANCESTOR_FPATH, std::string, "ancestor.gp", "Ancestor program file."),
  GROUP(HARDWARE_GROUP, "Hardware Settings."),
  VALUE(EVENT_DRIVEN, bool, true, "True: event-driven hardware; False: imperative hardware."),
  VALUE(INBOX_CAPACITY, size_t, 8, "Capacity of agent message inbox. Only relevant for imperative hardware."),
  VALUE(HW_MAX_CORES, size_t, 8, "."),
  VALUE(HW_MAX_CALL_DEPTH, size_t, 128, "."),
  VALUE(HW_MIN_BIND_THRESH, double, 0.5, "."),
  GROUP(DEME_GROUP, "Deme Settings."),
  VALUE(DEME_WIDTH, size_t, 5, "Number of cells in width to make deme."),
  VALUE(DEME_HEIGHT, size_t, 5, "Number of cells in height to make deme."),
  VALUE(DEME_EVAL_TIME, size_t, 128, "How many updates to evaluate a deme when computing fitness."),
  GROUP(MUTATION_GROUP, "Mutation Settings"),
  VALUE(PROG_MAX_FUNC_CNT, size_t, 4, "."),
  VALUE(PROG_MAX_FUNC_LEN, size_t, 32, "."),
  VALUE(PROG_MAX_ARG_VAL, size_t, 16, "."),
  VALUE(PER_BIT__AFFINITY_FLIP_RATE, double, 0.05, "."),
  VALUE(PER_INST__SUB_RATE, double, 0.005, "."),
  VALUE(PER_FUNC__SLIP_RATE, double, 0.05, "."),
  VALUE(PER_FUNC__FUNC_DUP_RATE, double, 0.05, "."),
  VALUE(PER_FUNC__FUNC_DEL_RATE, double, 0.05, "."),
  GROUP(DATA_GROUP, "Data Collection Settings"),
  VALUE(SYSTEMATICS_INTERVAL, size_t, 100, "."),
  VALUE(POP_SNAPSHOT_INTERVAL, size_t, 100, "."),
  VALUE(DATA_DIRECTORY, std::string, "./", ".")
)

#endif
