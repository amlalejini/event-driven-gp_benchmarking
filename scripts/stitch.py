'''
stitch.py

Used to stitch resub experiment data with original submission data, leaving only a log
of the stitching -- no other traces.
'''

import argparse, os, errno, subprocess

# TODO: TEST!

executables = ["consensus", "pattern_matching"]

def main():
    # Setup the argument parser.
    parser = argparse.ArgumentParser(description="Script to stitch original data collection with resubbed data collection.")
    parser.add_argument("directory", type=str, help="Target experiment directory.")
    # Extract commandline args.
    args = parser.parse_args()
    # Collect commandline args.
    exp_dir = args.directory
    print "Experiment data directory: " + exp_dir

    # Grab a list of all extended runs.
    extended_runs = [d for d in os.listdir(exp_dir) if ":" in d]
    for ext_run in extended_runs:
        print "--> " + ext_run
        # Parse extended run directory name for some info.
        ext_start_update = int(ext_run.split(":")[-1])  # Here's the update from which this extended run actually started.
        base_run = ext_run.split(":")[0]                # Here's the original run.
        # Use above into to build some convenient paths.
        ext_run_dir = os.path.join(exp_dir, ext_run)
        base_run_dir = os.path.join(exp_dir, base_run)

        # Find the executable in extended run directory.
        executable = None
        for d in os.listdir(ext_run_dir):
            if (d in executables): executable = d
        if executable == None:
            print "Failed to find executable."
            exit(-1)

        # Translate all pop files by ext_start_update.
        pops = [p for p in os.listdir(ext_run_dir) if "pop_" in p]
        for pop in pops:
            true_update = int(pop.split("_")[-1]) + ext_start_update
            true_pop = "pop_" + str(true_update)
            cmd = "mv %s %s" % (os.path.join(ext_run_dir, pop), os.path.join(base_run_dir, true_pop))
            return_code = subprocess.call(cmd, shell=True)

        # --- Edit fitness file. ---
        ext_fitness_contents = None
        base_fitness_contents = None
        with open(os.path.join(ext_run_dir, "fitness.csv"), "r") as fp:
            ext_fitness_contents = fp.read().split("\n")[1:] # Grab fitness file lines. Throw out header.
        with open(os.path.join(base_run_dir, "fitness.csv"), "r") as fp:
            base_fitness_contents = fp.read().split("\n")
        # Translate extended fitness contents by start update.
        true_ext_contents = ""
        for line in ext_fitness_contents:
            if line.isspace() or line == "": continue
            true_update = int(line.split(",")[0]) + ext_start_update
            true_line = line.split(",")
            true_line[0] = str(true_update)
            true_ext_contents += ",".join(true_line) + "\n"
        # Merge base fitness contents with extended fitness contents.
        merged_contents = base_fitness_contents[0] + "\n"
        for line in base_fitness_contents[1:]:
            if line.isspace() or line == "": continue # Skip empty lines
            update = int(line.split(",")[0])
            if update < ext_start_update: merged_contents += line + "\n"
        merged_contents += true_ext_contents
        # Write out merged contents.
        with open(os.path.join(base_run_dir, "fitness.csv"), "w") as fp:
            fp.write(merged_contents)

        # --- Edit systematics file. ---
        ext_sys_contents = None
        base_sys_contents = None
        with open(os.path.join(ext_run_dir, "systematics.csv"), "r") as fp:
            ext_sys_contents = fp.read().split("\n")[1:] # Grab sys file lines. Throw out header.
        with open(os.path.join(base_run_dir, "systematics.csv"), "r") as fp:
            base_sys_contents = fp.read().split("\n") # Keep header here.
        # Translate extended systematics contents by start update.
        true_ext_contents = ""
        for line in ext_sys_contents:
            if line.isspace() or line == "": continue
            true_update = int(line.split(",")[0]) + ext_start_update
            true_line = line.split(",")
            true_line[0] = str(true_update)
            true_ext_contents += ",".join(true_line) + "\n"
        # Merge base systematics contents with extended systematics contents.
        merged_contents = base_sys_contents[0] + "\n"
        for line in base_sys_contents[1:]:
            if line.isspace() or line == "": continue # Skip empty lines
            update = int(line.split(",")[0])
            if update < ext_start_update: merged_contents += line + "\n"
        merged_contents += true_ext_contents
        # Write out merged contents.
        with open(os.path.join(base_run_dir, "systematics.csv"), "w") as fp:
            fp.write(merged_contents)

        # Move the executable over.
        cmd = "mv %s %s" % (os.path.join(ext_run_dir, executable), os.path.join(base_run_dir, executable))
        return_code = subprocess.call(cmd, shell=True)

        # Move run.log over to base directory (run:ext_start_update.log)
        cmd = "mv %s %s" % (os.path.join(ext_run_dir, "run.log"), os.path.join(base_run_dir, "run:%d.log" % ext_start_update))
        return_code = subprocess.call(cmd, shell=True)

        # Make note of stitching in log.
        with open(os.path.join(base_run_dir, "stitch.log"), "a") as fp:
            fp.write("stitch@%d" % ext_start_update)

        # TODO Clean up resub directory.
        cmd = "rm -rf %s" % ext_run_dir
        return_code = subprocess.call(cmd, shell=True)

if __name__ == "__main__":
    main()
