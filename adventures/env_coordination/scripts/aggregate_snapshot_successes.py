import argparse, os, copy, errno, subprocess

# 40k
default_update = 50000
aggregator_dump = "./aggregated_data"

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
    parser = argparse.ArgumentParser(description="Experiment cleanup script.")
    parser.add_argument("directory", type=str, help="Target directory to pull runs from.")
    parser.add_argument("group", type=str, help="What group of experiments does this belong to?")
    parser.add_argument("-u", "--update", type=int, help="Final update of experiment.")
    args = parser.parse_args()


    update = args.update if args.update else default_update
    exp_dir = args.directory
    group = args.group
    dump = os.path.join(aggregator_dump, group)
    mkdir_p(dump)


    runs = [d for d in os.listdir(exp_dir) if "_ES8_" in d]
    success_aggregate = "treatment,run_id,indiv_id,func_used,inst_entropy,score\n"
    for run in runs:
        run_dir = os.path.join(exp_dir, run)
        run_id = run.split("_")[-1]
        treatment = "_".join(run.split("_")[:-1])
        pop_dir = os.path.join(run_dir, "output", "pop_{}".format(str(update)))
        pop_stats_fpath = os.path.join(pop_dir, "pop_{}.csv".format(str(update)))
        pop_stats = None
        try:
            with open(pop_stats_fpath, "r") as fp:
                pop_stats = fp.readlines()
        except:
            print("Could not open pop stats file for: " + run_dir)
            continue
        # Collect the header info. 
        header = pop_stats[0].split(",")
        header_lu = {header[i].strip():i for i in range(0, len(header))}
        pop_stats = pop_stats[1:]
        # What does it mean for a run to be successful?
        # - Env_matches = 128
        # - Time_all_tasks_credited > 0
        def IsSuccess(entry):
            matches_env = int(entry[header_lu["env_matches"]]) == 128
            completes_tasks = int(entry[header_lu["time_all_tasks_credited"]]) > 0
            return matches_env and completes_tasks

        successful_lines = [line.strip().split(",") for line in pop_stats if IsSuccess(line.strip().split(","))]
        for line in successful_lines:
            org_id = line[header_lu["id"]]
            func_used = line[header_lu["func_used"]]
            inst_ent = line[header_lu["inst_entropy"]]
            score = line[header_lu["score"]]
            indiv_id = run_id + "_" + org_id
            # success_aggregate = "treatment,run_id,indiv_id,func_used,inst_entropy,score\n"
            success_aggregate += ",".join([treatment, run_id, indiv_id, func_used, inst_ent, score]) + "\n"
    # Write out successes. 
    with open(os.path.join(dump, "solutions.csv"), "w") as fp:
        fp.write(success_aggregate)
    
                        
        



if __name__ == "__main__":
    main()