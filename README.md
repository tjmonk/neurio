# neurio
Neurio Home Energy Monitor - VarServer interface

## Overview

The Neurio server periodically polls a Neurio Home Energy Monitor
and stores the power and energy consumption data in VarServer variables

The variables polled are:

| | |
|---|---|
| Variable | Description |
| /CONSUMPTION/L1/V | Line 1 Voltage (V) |
| /CONSUMPTION/L1/P | Line 1 Power (W) |
| /CONSUMPTION/L1/Q | Line 1 Reactive Power (Var) |
| /CONSUMPTION/L1/ENERGY_INP | Line 1 Energy Imported (Ws) |
| /CONSUMPTION/L2/V | Line 2 Voltage (V) |
| /CONSUMPTION/L2/P | Line 2 Power (W) |
| /CONSUMPTION/L2/Q | Line 2 Reactive Power (Var) |
| /CONSUMPTION/L2/ENERGY_INP | Line 2 Energy Imported (Ws) |
| /CONSUMPTION/TOTAL/P | Total Power (W) |
| /CONSUMPTION/TOTAL/Q | Total Reactive Power (Var) |
| /CONSUMPTION/TOTAL/ENERGY_INP | Total Energy Imported (Ws) |

The Neurio server polls the Neurio Home Energy Monitor using an HTTP GET
request with Basic AUTH.  It is not secure and should only be used on a trusted private network.

## Command Line Arguments

The neurio service can be configured using the following command line arguments:

| | |
|---|---|
| Argument | Description |
| -v | Enable verbose output |
| -h | Display command usage and quit |
| -a | Specify Neurio Sensor IP address |
| -u | Specify Neurio Basic Authentication Credentials |
| -p | Specify Neurio Sensor Polling Interval in seconds |


## Prerequisites

The iothub service requires the following components:

- varserver : variable server ( https://github.com/tjmonk/varserver )
- libtjson : JSON library ( https://github.com/tjmonk/libtjson )

## Build

```
./build.sh
```

## Set up the VarServer

```
varserver &

mkvar -t float -n /consumption/l1/v
mkvar -t uint16 -n /comsumption/l1/p
mkvar -t int16 -n /consumption/l1/q
mkvar -t uint64 -n /consumption/l1/energy_imp
mkvar -t float -n /consumption/l2/v
mkvar -t uint16 -n /comsumption/l2/p
mkvar -t int16 -n /consumption/l2/q
mkvar -t uint64 -n /consumption/l2/energy_imp
mkvar -t uint16 -n /comsumption/total/p
mkvar -t int16 -n /consumption/total/q
mkvar -t uint64 -n /consumption/total/energy_imp

```

## Run the Nerio Server

```
neurio -a 192.168.86.31 -u YWRtaW46VGptN25ldXJpbzEh -p 5 &

```
## Query the vars

```
vars -vn /CONSUMPTION/
```

```
/consumption/l1/v=118.841003
/consumption/l1/q=28
/consumption/l1/q=28
/consumption/l1/energy_imp=100212000485
/consumption/l2/v=119.796997
/consumption/l2/q=-79
/consumption/l2/q=-79
/consumption/l2/energy_imp=69174964343
/consumption/total/q=-51
/consumption/total/q=-51
/consumption/total/energy_imp=169386964852
```
