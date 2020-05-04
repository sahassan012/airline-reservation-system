# Airline Reservation System
## About
This program concurrently handles the following functionalities for a mini Airline Reservation System(ARS):
- Book : reservation of a seat on a light and issuing a ticket number
- Cancel : cancelation of a ticket
- Change : cancelation and booking of another flight atomically

These operations are capable of being performed by multiple threads without any external synchronization. 
All functionalities are handled internally with the use of locks.

## How it works
Each ticket is identified by user id, flight id, and ticket number. 
In the current setup, to get a ticket, an agent thread books the flight by calling book_flight().
The ARS then saves the userid, flightid, and ticketnumber in the array of flight data structure.
The flight data structure includes a lock and a conditional variable to properly handle multiple threads.  

test.c, main.c, and wait.c are all programs that test the efficiency and correctness of the system.

## How to run(from command line)
To create executable files, for all 3 programs, use Makefile by running:
```console
$ make
```
This should create three executable files in the program directory:
- test
- wait
- main

You may then run:
```console
$ ./test {# of threads}
$ ./wait {# of threads} 
$ ./main {# of threads}
```

