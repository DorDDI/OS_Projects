Simulator Of A Traffic Circle

Simulator using single process with multi-thread programming combined with mutual exclusion.

The traffic circle is built of square units where each one of them may contain one car at most at any given time. 
The size of the square’s side is N.
In the circle's corners, there are four "car generators".
Each car generator generates a new car once in a randomized time, which is picked uniformly at random between
MIN_INTER_ARRIVAL_IN_NS and MAX_INTER_ARRIVAL_IN_NS [ns].
A car that has been generated enters the circle only once the square to which it wants to enter and the square before it are free.
A car tries to progress to the next square every INTER_MOVES_IN_NS [ns]. 
Then, if the next square is free, the car moves there; else, it stays in its current slot, until the next square is free.
Also in the circle’s corners are 4 sinks. Each car moves around the circles.
When a car progresses from a square before a sink, it disappears from the simulation with probability FIN_PROB; and continues moving
around the circle with probability 1 - FIN_PROB.
A car may vanish in a sink only after it has finished moving along at least one side of the square.

The simulation takes SIM_TIME seconds.

10 times during the simulation, the program should take a snapshot of the circle - namely, check which squares have cars on them; and then print the results.

Every square secure by a mutex.
Every car generator runs in a different thread and run simultaneously.
The print function run on a unique thread.
Every car has its unique thread that in charge on the progress of the car in the circle.