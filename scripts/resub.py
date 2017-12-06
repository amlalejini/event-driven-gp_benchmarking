'''
resub.py
This script does the following:
    * Produces a list of unfinished jobs and the most recent update they can be restarted from.
    * Generate a set of qsub files able to resubmit unfinished jobs @ most recent update.
'''

import argparse, os, copy, errno

benchmarks = ["consensus", "pattern_matching"]
treatments = ["Imperative_MsgForking", "Imperative_MsgNonForking", "EventDriven_MsgForking"]
default_final_update = 50000
default_walltime = "04:00:00:00"
default_feature = "intel16"
default_mem = "8gb"

generated_qsub_loc = "./generated_qsubs"

run_params = {"consensus": {
                "Imperative_MsgForking": {
                    "EVENT_DRIVEN": 0,
                    "FORK_ON_MESSAGE": 1
                },
                "Imperative_MsgNonForking": {
                    "EVENT_DRIVEN": 0,
                    "FORK_ON_MESSAGE": 0
                },
                "EventDriven_MsgForking": {
                    "EVENT_DRIVEN": 1,
                    "FORK_ON_MESSAGE": 1
                },
              },
              "pattern_matching": {
                "Imperative_MsgForking": {
                    "EVENT_DRIVEN": 0,
                    "FORK_ON_MESSAGE": 1,
                    "DEME_PROP_FULL": 1,
                },
                "Imperative_MsgNonForking": {
                    "EVENT_DRIVEN": 0,
                    "FORK_ON_MESSAGE": 0,
                    "DEME_PROP_FULL": 1,
                },
                "EventDriven_MsgForking": {
                    "EVENT_DRIVEN": 1,
                    "FORK_ON_MESSAGE": 1,
                    "DEME_PROP_FULL": 1,
                },
                "EventDriven_MsgForking_PropSize1": {
                    "EVENT_DRIVEN": 1,
                    "FORK_ON_MESSAGE": 1,
                    "DEME_PROP_FULL": 0,
                    "DEME_PROP_SIZE": 1
                }
              }
             }

# Variables to fill out:
#   * [walltime]
#   * [feature]
#   * [mem]
#   * [job_name]
#   * [benchmark]
#   * [pop_update]
#   * [random_seed]
#   * RUN_FROM_EXISTING_POP
#   * EXISTING_POP_LOC
#   * [run_parameters]
base_qsub = """#!/bin/bash -login
### Configure job:
#PBS -l walltime=[walltime]
#PBS -l feature=[feature]
#PBS -l mem=[mem]
#PBS -N [job_name]

### Load modules:
module load powertools
module load GNU/5.2

### Setup some variables.
BENCHMARK=[benchmark]
BENCHMARK_DIR=/mnt/home/lalejini/data/signal-gp-benchmarking/${BENCHMARK}
BASE_RUN_DIR=${BENCHMARK_DIR}/${PBS_JOBNAME}
RUN_DIR=${BASE_RUN_DIR}:[pop_update]
CODE_DIR=/mnt/home/lalejini/devo_ws/signal-gp-benchmarking/${BENCHMARK}

### Change to working directory, do work.
mkdir -p ${RUN_DIR}

cd ${BENCHMARK_DIR}
cp ${BASE_RUN_DIR}/configs.cfg ${RUN_DIR}
cp ${CODE_DIR}/${BENCHMARK} ${RUN_DIR}
cd ${RUN_DIR}

./${BENCHMARK} -RANDOM_SEED [random_seed] -GENERATIONS [generations] -RUN_FROM_EXISTING_POP 1 -EXISTING_POP_LOC ${BASE_RUN_DIR}/pop_[pop_update] [run_parameters] > run.log
"""

def mkdir_p(path):
    """
    This is functionally equivalent to the mkdir -p [fname] bash command
    """
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise

def main():
    parser = argparse.ArgumentParser(description="Experiment resubmission script.")
    parser.add_argument("directory", type=str, help="Target experiment directory.")
    parser.add_argument("benchmark", type=str, help="What benchmark is this " + str(benchmarks) + "?")
    parser.add_argument("-u", "--update", type=int, help="Final update of experiment.")
    parser.add_argument("-l", "--list", action="store_true", help="Print a list unfinished jobs.")
    parser.add_argument("-g", "--generate_resubs", action="store_true", help="Generate a HPCC submission script for unfinished jobs.")
    parser.add_argument("--walltime", type=str, help="Resubmission walltime request.")
    parser.add_argument("--feature", type=str, help="Resubmission feature request.")
    parser.add_argument("--mem", type=str, help="Resubmission memory request.")
    args = parser.parse_args()

    # If given parameters below, use them. Otherwise, use defaults.
    #   Note: 0 is not a valid final update.
    final_update = args.update if args.update else default_final_update
    walltime_param = args.walltime if args.walltime else default_walltime
    feature_param = args.feature if args.feature else default_feature
    mem_param = args.mem if args.mem else default_mem

    exp_dir = args.directory
    print "Experiment data directory: " + exp_dir
    print "  Final update: " + str(final_update)

    benchmark = args.benchmark
    if (not benchmark in benchmarks):
        print "Not a valid benchmark!"
        exit(-1)

    # Generate a list of unfinished jobs and for each:
    #    - Get the most recent population snapshot for that job.
    #    - Get the final fitness save for the job.
    #    - Get the rep id (random number seed) for that job.
    runs = [d for d in os.listdir(exp_dir) if d.split("-")[0] in treatments]
    run_info = {}
    for run in runs:
        run_id = run.split("__")[-1]
        run_treat = "-".join(run.split("-")[:-1])
        run_dir = os.path.join(exp_dir, run)
        fitness_contents = None
        with open(os.path.join(run_dir, "fitness.csv"), "r") as fp:
            fitness_contents = fp.readlines()
        last_update = int(fitness_contents[-1].split(",")[0])
        # Figure out most recent population snapshot.
        snapshots = [int(d.split("_")[-1]) for d in os.listdir(run_dir) if "pop_" in d]
        bestshot = -1
        if len(snapshots): bestshot = max(snapshots)
        finished = True if (last_update >= final_update) else False

        run_info[run] = {}
        run_info[run]["run_id"] = run_id
        run_info[run]["treatment"] = run_treat
        run_info[run]["last_update"] = last_update
        run_info[run]["last_snapshot"] = bestshot
        run_info[run]["finished"] = finished

    # If -list, print out a list of experiment statuses.
    print "\nRun information: "
    if args.list:
        treatment_cnts = {t:[0, 0] for t in treatments}
        for run in run_info:
            info = run_info[run]
            treatment_cnts[info["treatment"]][1] += 1
            if (info["finished"]): treatment_cnts[info["treatment"]][0] += 1
            print "  Run: " + run
            print "    Finished? %s (%d, %d)" % (str(info["finished"]), info["last_update"], info["last_snapshot"])
        print "\n  Overview: "
        for treatment in treatment_cnts:
            print "    %s: %d/%d finished" % (treatment, treatment_cnts[treatment][0], treatment_cnts[treatment][1])

    if args.generate_resubs:
        mkdir_p(generated_qsub_loc)
        # For each unfinished job, generate a HPCC resubmission script.
        for run in run_info:
            # If not finished, generate a qsub file for it.
            if (not info["finished"]):
                qsub = copy.deepcopy(base_qsub)
                print "Generating qsub for: " + run
                # Figure out some required parameters.
                treatment = run_info[run]["treatment"]
                run_id = str(run_info[run]["run_id"])
                job_name = run  #  Job name: treament-run_id__run_id  -- This needs to match the base
                pop_update = str(run_info[run]["last_snapshot"])
                random_seed = str(run_info[run]["run_id"])
                generations = str(final_update - run_info[run]["last_snapshot"])
                run_parameters = ""
                for param in run_params[benchmark][treatment]:
                    run_parameters += " -%s %s" % (str(param), str(run_params[benchmark][treatment][param]))
                # Fill out parameters.
                qsub = qsub.replace("[walltime]", walltime_param)
                qsub = qsub.replace("[feature]", feature_param)
                qsub = qsub.replace("[mem]", mem_param)
                qsub = qsub.replace("[job_name]", job_name)
                qsub = qsub.replace("[benchmark]", benchmark)
                qsub = qsub.replace("[pop_update]", pop_update)
                qsub = qsub.replace("[random_seed]", random_seed)
                qsub = qsub.replace("[run_parameters]", run_parameters)
                qsub = qsub.replace("[generations]", generations)
                # Write everything out to file.
                with open(os.path.join(generated_qsub_loc, run + ":" + pop_update + ".qsub"), "w") as fp:
                    fp.write(qsub)


if __name__ == "__main__":
    main()
