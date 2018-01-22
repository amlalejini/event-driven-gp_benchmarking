'''
ChgEnv-specific script to extract final dominant program from pop files.

'''

import argparse, os, copy, errno


def main():
    parser = argparse.ArgumentParser(description="Experiment cleanup script.")
    parser.add_argument("directory", type=str, help="Target directory to clean up.")
    parser.add_argument("-u", "--update", type=int, help="Final update of experiment.")

    final_update = args.update if args.update else default_final_update
    exp_dir = args.directory

    print "Experiment data directory: " + exp_dir
    print "  Final update: " + str(final_update)

    runs = [d for d in os.listdir(exp_dir) if "ChgEnv" in d]
    for run in runs:
        run_dir = os.path.join(exp_dir, run)
        with open("pop_%d/pop_%d.pop" % (final_update, final_update), "r") as fp:
            final_pop = fp.read()
        final_pop.split("===")
        fdom = final_pop[0]
        with open("test.txt", "w") as fp:
            fp.write(fdom)
        exit()

if __name__ == "__main__":
    main()
