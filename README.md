# Signal GP Benchmarking
The goal of this project is to compare the evolvability of an event-driven genetic programming representation with an imperative version of the same representation.

Stretch goal: also include PushGP, CartesianGP on all tasks. (maybe in an extension... If everything gets thrown into MABE, would be easy :wink: :wink:)

General experiment setup: Signal GP with and without events.

Question: Does event-driven linear genetic programming offer advantages over traditional imperative linear genetic programming? Does it offer disadvantages?

What types of tasks might one form of linear GP outperform the other?

Can I show that rewiring is actually trivial? Rewiring function calls vs. specialized instructions.

Stretch: how do systems respond to perturbations (antagonistic and neutral)?

## Experiment Notes
* 50 replicates of each treatment.

## Todos:
- [ ] Consensus
  - [ ] Analyze mode
- [ ] Pattern matching:
  - [x] Add clumped vs. random propagules option.
  - [ ] Analyze mode.
  - [x] Update comments.
- [ ] Python utility scripts
  - [x] Check for job completion.
  - [x] Generate resub qsub files.
  - [x] Submit a directory of qsub files.
  - [x] Stitch together original submissions with resubmissions.
  - [x] Aggregate summary data.
  - [ ] R/Python analysis scripts.
- [ ] Do a comment sweep.

## Data Collection Pipeline
1. Use original .qsub files (in each benchmarking directory) to submit initial runs.
  * i.e. `>> qsub [treatment_name].qsub`
  * can use `>> resub.py [data dir] [benchmark] -l` to list out finished/unfinished jobs while running.
2. If anything doesn't finish in given time on HPCC or HPCC kills things off for some reason, use the resub script to generate the appropriate resubmission qsub scripts.
  * NOTE: only do this once all jobs of a benchmark have either finished or died.
  * Command for generating resub consensus benchmarking: `python resub.py /mnt/home/lalejini/data/signal-gp-benchmarking/consensus consensus -l -g --walltime 04:00:00:00 --feature intel16 --mem 8gb -u 50000`
  * Use qsub.py to do the mass .qsub submitting.
3. Once all resubs of a benchmark have finished. Stitch the resubs together with the original run data using the stitch.py script.
4. Repeat steps 2 and 3 until everything gets done.


## Benchmark Tasks/Environments
To evaluate usefulness of capturing the event-driven paradigm in genetic programming, I am benchmarking Signal GP with and without access to the event-driven paradigm on several environments/tasks.

Tasks (for sure): consensus, pattern matching </br>
Tasks (maybe): lawnmower, predator/prey, foraging

### Consensus/Leader Election
Pull problem description primarily from (Weise and Tang, 2012) and (Knoester et al., 2013).

Evolve populations of distributed systems. When evaluating a system, initialize all agents within the system with identical programs (homogeneous systems).

Mutation occurs on group replication. Two options for replication:
  1. EA style - evaluate entire population, assign fitnesses, tournament selection
  2. asynchronous - replicate on consensus (issue: bootstrapping evolution of consensus).  

#### Treatments
* Event-driven messaging.
* Imperative messaging + auto fork on message read.
* Imperative messaging + non-forking on message read.

#### Things to keep in mind:
* Fitness function
  - Pressure for achieving efficiency.
  - Pressure for achieving and maintaining consensus.

#### Problem description from (Weise and Tang, 2012):
Given network N of nodes n performing an election is as follows:
1. The IDs of nodes are unique numbers drawn from N<sub>0</sub> and the order imposed on them is the less-than relation.
2. A node does not know the IDs of other nodes.
3. At startup, a node n in N only nows its own ID id(n), which is stored in a dedicated variable, symbol, or memory cell.
4. During the election, each node n<sub>i</sub> in N will decide (vote) for a node n<sub>j</sub> in N to be elected. Voting involves storing the ID of the voted for node in a dedicated memory cell.

Objective function consideration: no restrictions on elected node vs. restricting to max/min ID

Group genetic make-up: homogeneous

Agents: Could message neighboring agents (though messages had lag). Check and set vote ID.

#### Problem description from (Knoester et al., 2013):

Agents: Agents have an orientation that they can change (i.e. rotation is possible). Agents can send/retrieve messages.

##### Treatments
* Individual-level replication:
  1. germline
    * When an individual in a deme replicates, it produces an identical offspring. Homogeneous/clonal demes. Mutations only introduced on deme replication.
  2. asexual
    * When an individual in a deme replicates, it is mutated.
  3. sexual
    * Sexual reproduction within-deme.
  4. HGT
    * Organisms replicate asexually, but there is HGT within-deme.
* Gene flow among demes
  - 2 treatments w/gene flow: Migration and Wilcox (will not worry about this)

##### Relevant instructions:
* send-msg: sends message to faced neighbor
* retrieve-msg: loads previously received message
* bcast1: sends message to all neighboring cells
* set-opinion: set vote
* get-opinion: get current vote
* collect-cell-data: get UID
* if-cell-data-changed: checks whether or not cell data has changed since last collecting cell data.
* get-neighborhood: loads a hidden register with a list of IDs of all neighboring organisms.
* if-neighborhood-changed: checks if caller's neighborhood is different from when last get-neighborhood was called.
* rotate-left-one
* rotate-right-one

#### References:
T. Weise and K. Tang, “Evolving Distributed Algorithms With Genetic Programming,” IEEE Trans. Evol. Computat., vol. 16, no. 2, pp. 242–265, Mar. 2012.

D. B. Knoester, H. J. Goldsby, and P. K. McKinley, “Genetic Variation and the Evolution of Consensus in Digital Organisms,” IEEE Trans. Evol. Computat., vol. 17, no. 3, pp. 403–417, May 2013.

### Pattern Matching
Distributed systems are tasked with expressing (and maintaining) a pre-determined pattern.

Pattern: French flag (of arbitrary orientation).

Extend the consensus environment in terms of agent capabilities (eliminate agent identifiers, though).

#### Treatments
* Event-driven messaging.
* Imperative messaging w/automatic forking.
* Imperative messaging w/out automatic forking.
* Event-driven messaging w/propagule size = 1. (exploratory treatment for tangential research)

#### References:
D. Federici and K. Downing, “Evolution and Development of a Multicellular Organism: Scalability, Resilience, and Neutral Complexification,” Artif. Life, vol. 12, no. 3, pp. 381–409, 2006.

### Changing Environment
Simple toy environment setup to demonstrate usefulness of event-driven computation.

### Treatments
* Event-driven + Active sensing
* Only event-driven
* Only active sensing

### Logic Operations
Simple logic environment where agents must compete to perform logical operations.

* Event-driven (w/environment)
  - Request input signal from environment.
    - Environment signals input.
* Imperative (w/environment)
  - Request input from environment directly.
* Both options available.

Expectation: event-driven only worse than non-event driven. 

Like Logic-9 in Avida.

## Benchmarking Tests
- Task success
- Robustness to perturbation
  - Perturbations:
    - Antagonism
    - Agent knock-out
  - Measures:
    - Task success post-perturbation
    - 'Evolvability' in response to perturbation
- Task efficiency
- Task 'evolvability'
  - Amount of time to achieve 'adequate' task performance.

## Experiment Measurements
### General:
- Task metrics:
  - Measure of success
  - Measure of efficiency (communication efficiency, time, etc.)
