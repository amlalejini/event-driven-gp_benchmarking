#ifndef CONSENSUS_CONFIG_H
#define CONSENSUS_CONFIG_H

#include "config/config.h"

EMP_BUILD_CONFIG( ConsensusConfig,
  GROUP(DEFAULT_GROUP, "General Settings"),
  VALUE(DEBUG_MODE, bool, false, "Should we output debug information?"),
  VALUE(RANDOM_SEED, int, 2, "Random number seed (0 for based on time)")
)

#endif
