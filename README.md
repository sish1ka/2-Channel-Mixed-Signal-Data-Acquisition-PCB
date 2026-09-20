# 2-Channel-Mixed-Signal-Data-Acquisition-PCB# 2-Channel Mixed-Signal Data Acquisition PCB

## Overview

This project involves the design of a 2-channel mixed-signal data acquisition system intended to measure 0–5 V analog signals and convert them into digital data for microcontroller-based processing and analysis.

Each analog input passes through input protection, voltage scaling, buffering, and active low-pass filtering before being sampled by a 12-bit ADC on an STM32 microcontroller.

The system is being designed and simulated using LTspice and will be implemented as a 2-layer PCB using KiCad. Embedded C will be used for microcontroller data acquisition and MATLAB will be used for data analysis.

## Objectives

* Design a 2-channel analog data acquisition system
* Accept 0–5 V analog input signals
* Scale the input signals to an ADC-compatible voltage range
* Implement input protection against abnormal voltage excursions
* Buffer the analog signals to minimize loading between circuit stages
* Implement 2nd-order active low-pass filtering
* Sample the conditioned signals using a 12-bit ADC
* Transfer acquired data through UART
* Verify circuit behavior through simulation
* Design and verify a 2-layer PCB
* Analyze acquired data using MATLAB

## Tools

* LTspice
* KiCad
* C
* MATLAB
* Git / GitHub

## Circuit Design

![Circuit Design](Documentation/schematic.png)

## Design Summary

| Parameter                 |                Value |
| ------------------------- | -------------------: |
| Number of channels        |                    2 |
| Input signal              |                0–5 V |
| ADC voltage range         |              0–3.3 V |
| ADC resolution            |               12-bit |
| Sampling rate             |       4 kS/s/channel |
| Useful bandwidth          |             0–100 Hz |
| Low-pass filter           | 2nd-order Sallen-Key |
| Filter cutoff             |              ~297 Hz |
| Nominal system gain       |            ~0.60 V/V |
| Maximum nominal ADC input |               ~3.0 V |
| Microcontroller           |        STM32G031K8T6 |
| Analog op-amp             |              MCP6004 |
| PCB layers                |                    2 |
| PCB dimensions            |          ~80 × 50 mm |
| UART baud rate            |          460800 baud |

## LTspice Simulation

### Verification Procedure

The analog front end is being verified using a node-by-node transient analysis.

Each verification step checks a specific electrical condition before proceeding to the next stage.

### 1. Voltage divider

**Purpose:**
Verify that the input protection resistor and voltage divider produce the expected scaled voltage for a 0–5 V input signal.

**Circuit Configuration:**

| Component                 |   Value |
| ------------------------- | ------: |
| Input protection resistor |    1 kΩ |
| R_TOP                     | 16.5 kΩ |
| R_BOTTOM                  |   10 kΩ |

**Test Signal:**

```text
SINE(2.5 2.5 10)
```

The input signal has a 2.5 V DC offset, 2.5 V amplitude, and 10 Hz frequency, resulting in a 0–5 V input waveform.

**Transient Analysis:**

```text
.tran 100m
```

The 100 ms simulation window corresponds to one complete cycle of the 10 Hz input signal.

**Procedure:**

1. Apply the 0–5 V test signal to the input.
2. Run a transient analysis for 100 ms in LTspice.
3. Probe the input node.
4. Confirm that the input signal ranges from 0 V to 5 V.
5. Probe the output of the voltage divider.
6. Confirm that the scaled signal ranges from approximately 0 V to 1.82 V.

**Expected Result:**

| Parameter             | Expected |
| --------------------- | -------: |
| Input minimum         |      0 V |
| Input maximum         |      5 V |
| Scaled output minimum |     ~0 V |
| Scaled output maximum |  ~1.82 V |

**Measured Result:**
`Vin = 0–5.00 V`
`Vscaled = 0–1.82 V`

**Result:** Pass

![Voltage Scaling Verification](Documentation/test-01-voltage-divider.png)

### 2. Diode Voltage Protection Verification

**Purpose:**
Verify that the protection diode limits abnormal negative voltage excursions at the scaled node.

A bipolar test signal will be applied to intentionally drive the input below the normal 0–5 V operating range.

**Test Signal:**

```text
SINE(0 5 10)
```

This produces a −5 V to +5 V input waveform.

**Procedure:**

1. Apply the bipolar test signal to the input.
2. Run a transient analysis for 100 ms in LTspice.
3. Probe the scaled node.
4. Observe the negative voltage excursion without the protection diode.
5. Enable the protection diode.
6. Repeat the simulation.
7. Compare the negative voltage excursion with and without the diode.

**Expected Result:**

The protection diode should conduct during the negative voltage excursion and limit the negative voltage at the protected node.

**Measured Result:**
Without the diode, the scaled node follows the negative input excursion. With the diode connected, the negative voltage is clamped and the scaled node no longer follows the negative excursion below the diode's forward-voltage region.

**Result:** Pass

![Negative Voltage Protection](Documentation/test-02-with-diode.png)

![Negative Voltage Protection](Documentation/test-02-without-diode.png)

### Results

| Verification                |                Theoretical | Measured | Error | Result  |
| --------------------------- | -------------------------: | -------: | ----: | ------- |
| Voltage scaling             |                   0–1.82 V | 0–1.82 V |     — | Pass    |
| Negative voltage protection | Negative excursion clamped |  Pending |     — | Pending |
| Buffer                      |                    Pending |  Pending |     — | Pending |
| Filter cutoff               |                    ~297 Hz |  Pending |     — | Pending |
| Complete analog chain       |                    Pending |  Pending |     — | Pending |
| PCB DRC errors              |                          0 |  Pending |     — | Pending |

## PCB Design

*PCB design will be completed after the analog front-end simulation and verification.*

### Components

*To be completed.*

### Connector Pinout

*To be completed.*

### Circuit Parameters

| Parameter        |                Value |
| ---------------- | -------------------: |
| Supply Voltage   |               +3.3 V |
| Input Range      |                0–5 V |
| ADC Range        |              0–3.3 V |
| Sampling Rate    |       4 kS/s/channel |
| Filter           | 2nd-order Sallen-Key |
| Cutoff Frequency |              ~297 Hz |

### KiCAD schematic

![KiCAD schematic](Documentation/KiCAD_schematic.png)

### PCB Layout

![2D\_PCB](Documentation/PCB_2D.png)

### 3D View

![3D PCB](Documentation/PCB_3D.png)

### Fabrication Outputs

Gerber and drill files will be generated using KiCad for PCB manufacturing.

The fabrication outputs will include:

* Front and back copper
* Front and back solder mask
* Silkscreen
* Board outline
* Drill files

The generated files will be available in `KiCad/Gerbers/`.

### PCB Design Verification

The PCB layout will be checked using KiCad's Design Rules Checker (DRC).

**Target:**

* DRC errors: 0
* Unconnected items: 0
* Board outline: ~80 × 50 mm

## Firmware

*To be completed.*

The STM32 firmware will be responsible for:

* ADC configuration
* ADC sampling
* DMA-based data acquisition
* UART data transmission

## MATLAB Data Analysis

*To be completed.*

MATLAB will be used to process and analyze the sampled ADC data.

## Files

* `Documentation/` — design images and verification results
* `KiCad/` — schematic and PCB design files
* `LTspice/` — circuit simulations
* `Firmware/` — STM32 firmware
* `MATLAB/` — data analysis scripts
* `BOM/` — bill of materials
* `Gerbers/` — manufacturing outputs
