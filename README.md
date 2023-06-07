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

