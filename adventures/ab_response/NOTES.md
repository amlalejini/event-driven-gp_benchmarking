# Adventure Overview

This adventure is a toy problem designed to explore function regulation in SignalGP. After initially exploring function regulation with the T-maze, we decided to take a step back from the added complexities of the T-maze problem and design a simpler problem to work out the kinks with (and hopefully better understand/demonstrate) function regulation. 

I'm calling this toy problem the AB response problem. In the AB response problem, there are two possible signals (each with a distinct tag) and two possible responses (A or B). Before an agent is evaluated, the appropriate response for each signal (A or B) is randomly determined (i.e. signal 1 ==> A and signal 2 ==> B or signal 1 ==> B and signal 2 ==> A). When an agent responds to a signal it is given feedback (in a variety of possible ways). Based on the feedback the agent can determine if their response was appropriate, and if not, adjust future responses to that signal (e.g. using function regulation). Each signal-response is considered a trial, and agents are evaluated for many trials per evaluation. Approximately halfway through a set of trials, we switch which responses are appropriate for each signal. Thus, successful agents must forget the previous signal-response mapping and learn the new one. 

## Special requirements
Requires a version of SignalGP that implements function regulation. This is not currently a standard feature. To use function regulation, you'll need the SGP-FUNC-REG branch of my Empirical fork (amlalejini/Empirical). The next iteration of SignalGP will include function regulation; though, not in the exact same form as what I currently have implemented. 

# Feedback Mechanisms
I haven't picked a feedback mechanism yet, so here are my considerations:

1) Give an action-response signal that is triggered immediately after any action that gives agents the opportunity to self-regulate post-response. 
2) Give feedback on action execution. 
    - ActionA(ARG1) &rightarrow; WM[ARG1] = Feedback
3) Give agents a GetResourceLevel instruction/sensor.
    - GetResourceLevel(ARG1) &rightarrow; WM[ARG1] = Feedback
    - This would require agents to get their resource level before and after an action to resolve if their response was net-positive or negative.
4) Give feedback for the last action on the next signal. 

The above considerations are currently in rank-order of what I'm leaning toward. 
