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
    parser.add_argument("-ff", "--final_fitness", action="store_true", help="Aggregate final fitness data from fitness.csv file.")
    parser.add_argument("-fot", "--fitness_over_time", action="store_true", help="Aggregate fitness over time from fitness.csv file.")
    args = parser.parse_args()

    data_directory = args.data_directory
    benchmark = args.benchmark

    # Get a list of all runs.
    runs = [d for d in os.listdir(data_directory) if "__" in d]
    runs.sort()

    if (args.final_fitness):
        print "\nAggregating final fitness information..."
        dump = os.path.join(aggregator_dump, benchmark)
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

    if (args.fitness_over_time):
        print "\nAggregating final fitness over time information..."
        # TODO



if __name__ == "__main__":
    main()
