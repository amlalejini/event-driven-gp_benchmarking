'''
ChgEnv-specific script to extract final dominant program from pop files.

'''

import argparse, os, copy, errno

default_final_update = 100000

def main():
    parser = argparse.ArgumentParser(description="Experiment cleanup script.")
    parser.add_argument("directory", type=str, help="Target directory to clean up.")
    parser.add_argument("-u", "--update", type=int, help="Final update of experiment.")
    parser.add_argument("-r", "--run_fdom_analysis", action="store_true", help="Run analysis on fdom.")
    args = parser.parse_args()

    final_update = args.update if args.update else default_final_update
    exp_dir = args.directory

    print "Experiment data directory: " + exp_dir
    print "  Final update: " + str(final_update)

    runs = [d for d in os.listdir(exp_dir) if "ChgEnv" in d]
    for run in runs:
        run_dir = os.path.join(exp_dir, run)
        with open(os.path.join(run_dir, "pop_%d/pop_%d.pop" % (final_update, final_update)), "r") as fp:
            final_pop = fp.read()
        final_pop = final_pop.split("===")
        fdom = final_pop[0]
        fdom_fpath = os.path.join(run_dir, "fdom.gp")
        with open(fdom_fpath, "w") as fp:
            fp.write(fdom)
        break

    if (args.run_fdom_analysis):
        for run in runs:
            run_dir = os.path.join(exp_dir, run)
            run_info = run..split("-")[0].split("_")
            ED = "0" if run_info[1] == "ED0" else "1"
            AS = "0" if run_info[2] == "AS0" else "1"
            ENV = run_info[-1][2:]
            print run, ED, AS, ENV


if __name__ == "__main__":
    main()
