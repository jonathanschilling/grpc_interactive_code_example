This is a test server program to simulate a ngspice webservice.

It implements the webservice interface via gRPC.
For a given time interval, it requests a voltage from the client
and returns 42.0 times that voltage (It's an amplifier ;-).

1) build the code

```
make
```

2) run the server

```
./server &
```

3) run the client

```
./client > run.dat
```

4) inspect the results
remove all commenting lines from run.dat

```
gnuplot --persist plot_output.plt
```
