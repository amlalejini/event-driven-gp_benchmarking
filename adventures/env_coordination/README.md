# Adventure Overview
The environment coordination experiment is designed to be a simple toy problem that tests the capacity for SignalGP agents to coordinate with external signals. In several papers, I've referred to this problem as the Changing Environment problem. However, I'm thinking that environment coordination might be a more apt name; I'll likely use both names interchangeably. 

# Changing Environment Task/Environment Coordination Task
The changing environment problem requires agents to coordinate their behavior with a randomly changing environment. The environment can be in one of *K* possible states; to maximize fitness, agents must match their internal state to the current state of their environment. The environment is initialized to a random state and has a configurable chance of changing to a random state at every subsequent time step. Successful agents must adjust their internal state whenever an environmental change occurs. 

The problem scales in difficulty as *K* increases. Agents adjust their internal state by executing one of *K* state-altering instructions. For each possible environmental state, there is an associated SetState instruction. Being required to execute a distinct instruction for each environment represents performing a behavior unique to that environment. 

Agents can monitor environmental changes in a two ways: 1) by responding to signals produced by environmental changes and 2) by using sensory instructions to poll the environment for changes.

Additional (configurable) tasks in this experiment code:
- Handling distraction signals
- Performing 10 different one and two input logic operations (ECHO, NAND, ...., EQUALS). 

I've used this or a variant of this problem in the following papers: (Lalejini and Ofria, 2018). 

# TODOs

# References
Lalejini, A., & Ofria, C. (2018). Evolving Event-driven Programs with SignalGP. In Proceedings of the Genetic and Evolutionary Computation Conference. ACM. https://doi.org/10.1145/3205455.3205523