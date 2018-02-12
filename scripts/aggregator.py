'''
aggregator.py
This script does the following:
    * Aggregate final fitness data for a benchmark into a .csv:
        - benchmark, treatment, run_id, final_update, mean_fitness, max_fitness
'''


import argparse, os, copy, errno

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
    parser = argparse.ArgumentParser(description="Data aggregation script.")
    parser.add_argument("data_directory", type=str, help="Target experiment directory.")
    parser.add_argument("benchmark", type=str, help="What benchmark is this?")

    parser.add_argument("-u", "--update", type=int, help="Update to get fitness for. If no update is given, last line of fitness file is used.")
    parser.add_argument("-f", "--fitness_file", type=str, help="File to pull fitness from.")

    # OLD Options.
    parser.add_argument("-ff", "--final_fitness", action="store_true", help="Aggregate final fitness data from fitness.csv file.")
    parser.add_argument("-fot", "--fitness_over_time", action="store_true", help="Aggregate fitness over time from fitness.csv file.")
    parser.add_argument("-mt_ff", "--multi_trial_final_fitness", action="store_true", help="Aggregate multi-trial final fitness data.")
    parser.add_argument("-tease_ff", "--teaser_final_fitness", action="store_true", help="Aggregate teaser final fitness data.")
    args = parser.parse_args()

    data_directory = args.data_directory
    benchmark = args.benchmark
    dump = os.path.join(aggregator_dump, benchmark)

    # Get a list of all runs.
    runs = [d for d in os.listdir(data_directory) if "__" in d]
    runs.sort()

    fit_update = args.update
    fit_file = args.fitness_file

    if (fit_file):
        print "\nExtracting fitness information..."
        mkdir_p(dump)
        f_content = "benchmark,treatment,run_id,update,mean_fitness,max_fitness\n"
        for run in runs:
            print "  run: " + str(run)
            run_id = run.split("__")[-1]
            run_treat = run.split("__")[0]
            run_dir = os.path.join(data_directory, run)
            with open(os.path.join(run_dir, fit_file), "r") as fp:
                fitness_contents = fp.readlines()
            header = fitness_contents[0].split(",")
            header_lu = {header[i].strip():i for i in range(0, len(header))}
            fitness_contents = fitness_contents[1:]
            fu_content = []
            if (fit_update != None):
                # Look for give update.
                target_line = None
                for line in fitness_contents:
                    line = line.split(",")
                    if (line[header_lu["update"]] == str(fit_update)):
                        target_line = line
                        break
                if target_line:
                    fu_content = map(lambda x : x.strip(), target_line)
                else:
                    print "Failed to find fitness @ update %d" % fit_update
                    exit(-1)
            else:
                # Just use the last line.
                fu_content = fitness_contents[-1].split(",")
                fu_content = map(lambda x : x.strip(), fu_content)
            # Now we're ready to extract info!
            final_update = fu_content[header_lu["update"]]
            mean_fitness = fu_content[header_lu["mean_fitness"]]
            max_fitness = fu_content[header_lu["max_fitness"]]
            f_content += ",".join([benchmark, run_treat, run_id, final_update, mean_fitness, max_fitness]) + "\n"
        with open(os.path.join(dump, "fitness_%s.csv"), "w") as fp:
            fp.write(f_content)


    if (args.final_fitness):
        print "\nAggregating final fitness information..."
        mkdir_p(dump)
        ff_content = "benchmark,treatment,run_id,final_update,mean_fitness,max_fitness\n"
        for run in runs:
            print "  run: " + str(run)
            # benchmark, treatment, run_id, final_update, mean_fitness, max_fitness
            run_id = run.split("__")[-1]
            run_treat = "-".join(run.split("-")[:-1])
            run_dir = os.path.join(data_directory, run)
            with open(os.path.join(run_dir, "fitness.csv"), "r") as fp:
                fitness_contents = fp.readlines()
            header = fitness_contents[0].split(",")
            header_lu = {header[i].strip():i for i in range(0, len(header))}
            fu_content = fitness_contents[-1].split(",")
            fu_content = map(lambda x : x.strip(), fu_content)
            # Now we're ready to extract info!
            final_update = fu_content[header_lu["update"]]
            mean_fitness = fu_content[header_lu["mean_fitness"]]
            max_fitness = fu_content[header_lu["max_fitness"]]
            ff_content += ",".join([benchmark, run_treat, run_id, final_update, mean_fitness, max_fitness]) + "\n"
        with open(os.path.join(dump, "final_fitness.csv"), "w") as fp:
            fp.write(ff_content)

    if (args.multi_trial_final_fitness):
        print "\nAggregating multi-trial final fitness information..."
        mkdir_p(dump)
        ff_content = "benchmark,treatment,run_id,fitness\n"
        for run in runs:
            print "  run: " + str(run)
            # benchmark, treatment, run_id, final_update, mean_fitness, max_fitness
            run_id = run.split("__")[-1]
            run_treat = "-".join(run.split("-")[:-1])
            run_dir = os.path.join(data_directory, run)

            with open(os.path.join(run_dir, "fdom.csv"), "r") as fp:
                fitness_contents = fp.readlines()
            header = fitness_contents[0].split(",")
            header_lu = {header[i].strip():i for i in range(0, len(header))}
            fitness_contents = fitness_contents[1:]
            # Aggregate data.
            trials = 0
            fit_agg = 0
            for line in fitness_contents:
                line = line.split(",")
                fitness = float(line[header_lu["fitness"]])
                trials += 1
                fit_agg += fitness
            agg_fitness = fit_agg / float(trials)
            ff_content += ",".join([benchmark, run_treat, run_id, str(agg_fitness)]) + "\n"
        with open(os.path.join(dump, "mt_final_fitness.csv"), "w") as fp:
            fp.write(ff_content)

    if (args.teaser_final_fitness):
        print "\nAggregating teaser, multi-trial final fitness information..."
        mkdir_p(dump)
        ff_content = "benchmark,treatment,teaser,run_id,fitness\n"
        for run in runs:
            if not "ChgEnv_ED1_AS1_P125" in run: continue
            print "  run: " + str(run)

            run_id = run.split("__")[-1]
            run_treat = "-".join(run.split("-")[:-1])
            run_dir = os.path.join(data_directory, run)

            with open(os.path.join(run_dir, "teaser_sensors.csv"), "r") as fp:
                ts_contents = fp.readlines()
            header = ts_contents[0].split(",")
            header_lu = {header[i].strip():i for i in range(0, len(header))}
            ts_contents = ts_contents[1:]
            # Aggregate data.
            trials = 0
            fit_agg = 0
            for line in ts_contents:
                line = line.split(",")
                fitness = float(line[header_lu["fitness"]])
                trials += 1
                fit_agg += fitness
            agg_fitness = fit_agg / float(trials)
            ff_content += ",".join([benchmark, run_treat, "sensors", run_id, str(agg_fitness)]) + "\n"

            with open(os.path.join(run_dir, "teaser_events.csv"), "r") as fp:
                ts_contents = fp.readlines()
            header = ts_contents[0].split(",")
            header_lu = {header[i].strip():i for i in range(0, len(header))}
            ts_contents = ts_contents[1:]
            # Aggregate data.
            trials = 0
            fit_agg = 0
            for line in ts_contents:
                line = line.split(",")
                fitness = float(line[header_lu["fitness"]])
                trials += 1
                fit_agg += fitness
            agg_fitness = fit_agg / float(trials)
            ff_content += ",".join([benchmark, run_treat, "events", run_id, str(agg_fitness)]) + "\n"

            with open(os.path.join(run_dir, "fdom.csv"), "r") as fp:
                ts_contents = fp.readlines()
            header = ts_contents[0].split(",")
            header_lu = {header[i].strip():i for i in range(0, len(header))}
            ts_contents = ts_contents[1:]
            # Aggregate data.
            trials = 0
            fit_agg = 0
            for line in ts_contents:
                line = line.split(",")
                fitness = float(line[header_lu["fitness"]])
                trials += 1
                fit_agg += fitness
            agg_fitness = fit_agg / float(trials)
            ff_content += ",".join([benchmark, run_treat, "none", run_id, str(agg_fitness)]) + "\n"

        with open(os.path.join(dump, "teaser_final_fitness.csv"), "w") as fp:
            fp.write(ff_content)

    if (args.fitness_over_time):
        print "\nAggregating final fitness over time information..."
        # Some repeated work... but lazy. Shit's fast enough to get by.
        mkdir_p(dump)
        fot_content = "benchmark,treatment,run_id,update,mean_fitness,max_fitness\n"
        for run in runs:
            print "  run: " + str(run)
            # benchmark, treatment, run_id, final_update, mean_fitness, max_fitness
            run_id = run.split("__")[-1]
            run_treat = "-".join(run.split("-")[:-1])
            run_dir = os.path.join(data_directory, run)
            with open(os.path.join(run_dir, "fitness.csv"), "r") as fp:
                fitness_contents = fp.readlines()
            header = fitness_contents[0].split(",")
            header_lu = {header[i].strip():i for i in range(0, len(header))}
            for line in fitness_contents[1:]:
                line = map(lambda x : x.strip(), line.split(","))
                update = line[header_lu["update"]]
                mean_fit = line[header_lu["mean_fitness"]]
                max_fit = line[header_lu["max_fitness"]]
                fot_content += ",".join([benchmark, run_treat, run_id, update, mean_fit, max_fit]) + "\n"
        with open(os.path.join(dump, "fitness_over_time.csv"), "w") as fp:
            fp.write(fot_content)



if __name__ == "__main__":
    main()
