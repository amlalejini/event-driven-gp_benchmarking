# Event-driven GP Benchmarking
The goal of this project is to compare the evolvability of an event-driven genetic programming representation with an imperative version of the same representation. Stretch goal: also include PushGP, CartesianGP on all tasks. (maybe in an extension... If everything gets thrown into MABE, would be easy :wink: :wink:)

General experiment setup: Signal GP with and without events.

Does event-driven linear genetic programming offer advantages over traditional imperative linear genetic programming? Disadvantages?

What types of tasks might one form of linear GP outperform the other?

Can I show that rewiring is actually trivial? Rewiring function calls vs. specialized instructions.

Stretch: how do systems respond to perturbations (antagonistic and neutral)?

## Benchmark Tasks/Environments
### Consensus/Leader Election
Pull problem description primarily from (Weise and Tang, 2012) and (Knoester et al., 2013).

Evolve populations of distributed systems. When evaluating a system, initialize all agents within the system with identical programs (homogeneous systems).

Mutation occurs on group replication. Two options for replication: (1) EA style - evaluate entire population, assign fitnesses, tournament selection, (2) asynchronous - replicate on consensus.  

###### Things to keep in mind:
* Pressure for achieving efficiency. (+ [Total Time - Time for consensus])
* Likely fitness function: max(supported ID) + [total time - time in which consensus is achieved]
  - What about maintaining consensus?

##### Problem description from (Weise and Tang, 2012):
Given network N of nodes n performing an election is as follows:
1. The IDs of nodes are unique numbers drawn from N<sub>0</sub> and the order imposed on them is the less-than relation.
2. A node does not know the IDs of other nodes.
3. At startup, a node n in N only nows its own ID id(n), which is stored in a dedicated variable, symbol, or memory cell.
4. During the election, each node n<sub>i</sub> in N will decide (vote) for a node n<sub>j</sub> in N to be elected. Voting involves storing the ID of the voted for node in a dedicated memory cell.

Objective function consideration: no restrictions on elected node vs. restricting to max/min ID

Group genetic make-up: homogeneous

Agents: Could message neighboring agents (though messages had lag). Check and set vote ID.

##### Problem description from (Knoester et al., 2013):

Agents: Agents have an orientation that they can change (i.e. rotation is possible). Agents can send/retrieve messages.

###### Treatments
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

###### Relevant instructions:
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

### Lawnmower Problem
This problem is described and used in both a modified and original form in Spector et al. 2011.

##### Problem Description from (Spector et al., 2011) and (Koza, 1992)

NOTE: will want to checkout original specification of problem.

**Objective:** completely 'mow' a virtual lawn with a programmable virtual lawnmower.

**Setup:** An NxN grid that contains lawn squares and obstacles (if obstacles are present). The world is toroidal.

###### Agents capabilities:
* Actuation: rotation, move forward
* Sensing: obstacle/grass sensor
* Instruction set (additions):
  * rotate
  * mow
  * V8A
  * frog -- one-argument operator that jumps the lawnmower ahead and sideways an amount indicated by its vector argument (while mowing the destination).
  * progn -- two argument sequencing function



#### References:
L. Spector, B. Martin, K. Harrington, and T. Helmuth, Tag-based modules in genetic programming. New York, New York, USA: ACM, 2011, pp. 1419–1426.

Koza, J. R. (1992). Genetic programming: on the programming of computers by means of natural selection (Vol. 1). MIT press.

### Pattern Matching

#### References:
D. Federici and K. Downing, “Evolution and Development of a Multicellular Organism: Scalability, Resilience, and Neutral Complexification,” Artif. Life, vol. 12, no. 3, pp. 381–409, 2006.

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
- Task 'Evolvability'
  - Amount of time to achieve 'adequate' task performance.

## Experiment Measurements
### General:
- Task metrics:
  - Measure of success
  - Measure of efficiency (communication efficiency, time, etc.)
