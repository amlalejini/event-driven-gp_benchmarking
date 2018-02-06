'''
cleanup.py
This script cleans up unnecessary files from runs on HPCC.
'''

import argparse, os, copy, errno


def main():
    parser = argparse.ArgumentParser(description="Experiment cleanup script.")
    parser.add_argument("directory", type=str, help="Target directory to clean up.")
    parser.add_argument("-u", "--update", type=int, help="Final update of experiment.")
    args = parser.parse_args()

    final_update = args.update if args.update else default_final_update
    exp_dir = args.directory
    update = args.update

    print "Experiment data directory: " + exp_dir
    print "  Final update: " + str(final_update)

    runs = [d for d in os.listdir(exp_dir) if "__" in d]
    for run in runs:
        print "Cleaning up run: " + str(run)
        run_dir = os.path.join(exp_dir, run)
        things = [d for d in os.listdir(run_dir)]
        for thing in things:
            if "pop_" in thing and thing != "pop_%d" % update:
                cmd = "rm -rf %s" % os.path.join(run_dir, thing)
                return_code = subprocess.call(cmd, shell=True)
            if thing == "pattern_matching" or thing == "consensus" or thing == "changing_environment":
                cmd = "rm %s" % os.path.join(run_dir, thing)
                return_code = subprocess.call(cmd, shell=True)






if __name__ == "__main__":
    main()
