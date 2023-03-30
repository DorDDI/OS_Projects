Simulator Of A Basic Memory System

Simulator using multi-thread and multi-process programming combined with mutual exclusion, signals, condition variable and message system.

Each process simulates a real process which run on the CPU that read from MEM and wait for ACK.

HD - process that simulate the HD that receive requests, wait t HD_ACCS_T [ns] and send the requester an indication, that the request was “done”.

The MMU unit - The memory contains N pages and will be in charge on the pages in the memory and including 3 threads:

1. The main thread - Receives requests from processes and in charge of the page fault system (hit/miss).
• If the memory is empty, the request is a miss (page fault).
• If the memory isn’t empty, the request is a hit with probability 0 < HIT_RATE < 1, and
a miss otherwise.
• In case of a read hit, immediately acknowledge the requesting process that the access
was “done”.
• In case of a hit write
	• Sleep for MEM_WR_T [ns]
	• Choose uniformly at random one of the used pages in the memory, and mark it as dirty.
	• Acknowledge the requesting process that the access was “done”.
• In case of a miss (page fault)
	• If the memory is full
		▪ Wake up the evicter (to be described shortly)
		▪ Wait until the evicter wakes me up again, indicating that the memory is not full anymore.
	• Once the memory is not full, the thread sends the HD (hard disk) a request to read a page. 
	After receiving an acknowledge from the HD, the thread “writes” the page to the memory and acknowledges the requesting process, same as described above in the case of a hit.

2. Evicter - Chooses which page to evict in a FIFO manner, using the clock scheme.
The evicter is woken up by the main thread every time the memory is full.
If the page is dirty, the eviction requires sending a request and receive an ack from the HD.
If the number of used slots in the memory is N-1 (namely, the memory was full before evicting),
the evicter wakens up the main thread, to let him load a page, if needed.
In any case, the evicter continues evicting pages, until the number of the used slots in the
memory is below USED_SLOTS_TH. Then, the evicter stops evicting, and waits for the main
thread to wake it up again.

3. Printer -  Prints the memory every TIME_BETWEEN_SNAPSHOTS [ns].
Every slot in the memory is marked by 0 if it’s valid and clean; 1 if it’s valid and dirty; and – if it’s invalid (empty).

The simulation takes SIM_TIME seconds. Later, the message "Successfully finished sim" should be printed, and the simulation should be finished.