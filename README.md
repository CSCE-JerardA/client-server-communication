### Copyright 2026 JC Austin 
# Project 2: Local IPC and Resource Management


## Overview
This project implements a concurrent server that receives file hashing 
requests from clients via Unix domain datagram sockets, processes them 
using a pool of file readers and SHA solvers, and returns results via 
Unix domain stream sockets.

## How to Build
make

## How to Run
./bin/proj2-server <socket_path> <num_readers> <num_solvers>

Example:
./bin/proj2-server /tmp/jcaustin_sock 4 16

## Design Decisions
* Used a producer/consumer model with a mutex-protected queue and semaphore
  to dispatch work to worker threads
* Worker threads wait on semaphore until a message is available
* Deadlock is prevented by always acquiring solvers before readers,
  ensuring a consistent resource acquisition order
* Results are returned to the client via a Unix domain stream connection

## Known Issues
* Not likely to hold more than 50+ or 100+ solvers/readers

