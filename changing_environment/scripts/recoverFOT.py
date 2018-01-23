'''
recoverFOT.py

Script to recover fitness over time data. Fucked up and nuked fitness.csv and systematics.csv
for changing environment runs. Still have populations, and can recover fitness data from run logs,
so nothing necessary was truly lost. Just a pain in the ass.
'''
import argparse, os, copy, errno

default_interval = 100
default_final_update = 100000

def main():
    parser = argparse.ArgumentParser(description="Experiment cleanup script.")
    parser.add_argument("directory", type=str, help="Target directory to clean up.")
    parser.add_argument("-u", "--update", type=int, help="Final update of experiment.")
    parser.add_argument("-i", "--interval", type=int, help="Final update of experiment.")
    args = parser.parse_args()

    final_update = args.update if args.update else default_final_update
    interval = args.interval if args.interval else default_interval
    exp_dir = args.directory

    runs = [d for d in os.listdir(exp_dir) if "ChgEnv" in d]
    for run in runs:
        print "Recovering: " + run
        run_dir = os.path.join(exp_dir, run)
        with open(os.path.join(run_dir, "run.log"), "r") as fp:
            log = fp.read()
        fit_log = log.split("-------------------------")[-1]
        fit_log = fit_log.split("\n")
        content = "update,mean_fitness,min_fitness,max_fitness,inferiority\n"
        for line in fit_log:
            line = line.split("  ")
            update = int(line[0].split(" ")[-1])
            score = int(line[-1].split(" ")[-1])
            if update % interval == 0:
                content += ",".join([str(update), str(0), str(0), str(score), str(0)]) + "\n"
        with open(os.path.join(run_dir, "recovered_fitness.csv"), "w") as fp:
            fp.write(content)


if __name__ == "__main__":
    main()
