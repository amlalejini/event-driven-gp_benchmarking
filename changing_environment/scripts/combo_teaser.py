'''
Script to tease apart combined condition results.


'''

import argparse, os, copy, errno, subprocess

default_final_update = 100000

def main():
    parser = argparse.ArgumentParser(description="Experiment cleanup script.")
    parser.add_argument("directory", type=str, help="Target directory to clean up.")
    parser.add_argument("-u", "--update", type=int, help="Final update of experiment.")
    args = parser.parse_args()

    final_update = args.update if args.update else default_final_update
    exp_dir = args.directory

    print "Experiment data directory: " + exp_dir
    print "  Final update: " + str(final_update)

    runs = [d for d in os.listdir(exp_dir) if "ChgEnv_ED1_AS1_P125" in d]

    for run in runs:
        run_dir = os.path.join(exp_dir, run)
        run_info = run.split("-")[0].split("_")
        ENV = run_info[-1][3:]
        RND = run.split("__")[-1]
        args = {"ENVIRONMENT_STATES": ENV,
                "EVENT_DRIVEN": "1",
                "ACTIVE_SENSING": "1",
                "RANDOM_SEED": RND,
                "ENVIRONMENT_CHG_PROB":"0.125",
                "HW_MAX_CORES":"32",
                "ANALYZE_MODE":"1",
                "ANALYSIS":"1",
                "ANALYZE_AGENT_FPATH":"fdom.gp",
                "FDOM_ANALYSIS_TRIAL_CNT":"100",
                "TEASER_SENSORS": "1",
                "TEASER_EVENTS": "0"
                }

        # Copy executable to run dir.
        cp_cmd = "cp /mnt/home/lalejini/devo_ws/signal-gp-benchmarking/changing_environment/changing_environment %s" % run_dir
        return_code = subprocess.call(cp_cmd, shell=True)

        # Run w/teaser sensors
        arg_str = ["-%s %s" % (key, args[key]) for key in args]
        cmd = "./changing_environment " + " ".join(arg_str)
        print "Running: " + cmd
        return_code = subprocess.call(cmd, shell = True, cwd = run_dir)

        # Run w/teaser events
        args["TEASER_EVENTS"] = "1"
        args["TEASER_SENSORS"] = "0"
        arg_str = ["-%s %s" % (key, args[key]) for key in args]
        cmd = "./changing_environment " + " ".join(arg_str)
        print "Running: " + cmd
        return_code = subprocess.call(cmd, shell = True, cwd = run_dir)

if __name__ == "__main__":
    main()
