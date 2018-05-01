
# Adventure Overview
I'll use this adventure to test SignalGP function regulation development. 

The SignalGP function regulation demonstrated by this adventure is proof of concept. A more generic version of function regulation is a planned feature of the next version of SignalGP. 

## Special requirements
Requires version of SignalGP that implements function regulation. This is not currently a standard feature. To use function regulation, you'll need the SGP-FUNC-REG branch of my Empirical fork (amlalejini/Empirical). The next iteration of SignalGP will include function regulation; though, not in the exact same form as what I currently have implemented. 

## Details to sweep:
- Apply minimum similarity threshold to regulation references? 
- Apply regulation modifier to regulation references?
- Should up/down regulate amount be constant? Argument-specified?
- How should we apply up/down regulation (mutiplication, addition, etc)?