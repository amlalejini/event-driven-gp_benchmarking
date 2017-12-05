'''
qsub.py

A script to submit all .qsub files in a directory.

'''
import os, errno, argparse, subprocess

subbed_dir = "submitted"

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
    # Setup argument parser.
    parser = argparse.ArgumentParser(description="Experiment qsub subber.")
    parser.add_argument("directory", type=str, help="Target .qsub directory.")
    # Collect commandline args.
    args = parser.parse_args()
    # Extract commandline arguments.
    qsub_dir = args.directory
    print "Target qsub directory: " + qsub_dir

    # Collect all of the qsubs to submit.
    qsubs = [f for f in os.listdir(qsub_dir) if ".qsub" in f]
    mkdir_p(os.path.join(qsub_dir, subbed_dir))
    for qsub in qsubs:
        # Move qsub into submission directory.
        cmd = "mv %s %s" % (os.path.join(qsub_dir, qsub), os.path.join(qsub_dir, subbed_dir, qsub))
        return_code = subprocess.call(cmd, shell=True)
        # Submit the qsub.
        cmd = "qsub %s" % (os.path.join(qsub_dir, subbed_dir, qsub))
        return_code = subprocess.call(cmd, shell=True)
    print "Done!"

if __name__ == "__main__":
    main()
