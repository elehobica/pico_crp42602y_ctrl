# Raspberry Pi Pico CRP42602Y mechanism control

## Overview
Control library for CRP42602Y auto-reverse cassette tape deck mechanism

![sample application](doc/sample_app.gif)

### Features
* Generate solenoid pull timing for the function gear of CRP42602Y to support Play A/B, Cueing and Stop control
* Perform auto-stop action by rotation sensor of CRP42602Y
* Support 3 auto-reverse modes (One way, One round and Infinite round)
* Support timeout power disable to stop motor when no operations (optional)
* Provide commands and callbacks for user interface

## Supported Board and Peripheral Devices
* Raspberry Pi Pico (rp2040)
* CRP42602Y mechanism
* Need additional circuit to drive solenoid

## Pin Assignment & Connection
### CRP42602 connector
| Pin # | Name | Function | Connection |
----|----|----|----
| 1 | REC_B_SW | Rec. protection switch of B side<br>(0: not protected, 1: protected) | to Pico GPIO input |
| 2 | ROTATION_SENS | Rotation sensor | to Pico GPIO input |
| 3 | GEAR_STATUS_SW | Gear status switch<br>(0: in function, 1 not in function) | to Pico GPIO input |
| 4 | SOLENOID_1 | Solenoid coil 1 | from solenoid driver circuit |
| 5 | SOLENOID_2 | Solenoid coil 2 | from solenoid driver circuit |
| 6 | GND | GND | GND |
| 7 | CASSETTE_DETECT_SW | Cassette detection switch<br>(0: detected, 1: not detected) | to Pico GPIO input |
| 8 | P12V | 12V Power for motor and<br>IR LED for rotation sensor | from power circuit |
| 9 | REC_A_SW | Rec. protection switch of A side<br>(0: not protected, 1: protected) | to Pico GPIO input |

### Raspberry Pi Pico
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 4 | GP2 | GPIO Output | to SOLENOID_CTRL of additional circuit (0: release, 1: pull) |
| 5 | GP3 | GPIO Input | CASSETTE_DETECT from CRP42602Y pin 7 |
| 6 | GP4 | GPIO Input | GEAR_STATUS_SW from CRP42602Y pin 3 |
| 7 | GP5 | GPIO Input | ROTATION_SENS from CRP42602Y pin 2 |
| 8 | GND | GND | GND |
| 9 | GP6 | GPIO Output | to POWER_CTRL (optional) (0: disable, 1: enable) |

Pin assignments are configurable

## Schematic
### Control circuit schematic
[CRP62402Y_ctrl_schematic](doc/CRP62402Y_ctrl_schematic.pdf)

The solenoid driver part is needed at least.

### Head circuit (for reference)
[CRP62402Y_head_circuit](doc/CRP42602Y_head_circuit.png)

Polarity information is relative, however, L/R polarities should be aligned.

## How to build
* See ["Getting started with Raspberry Pi Pico"](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
* Put "pico-sdk", "pico-examples" and "pico-extras" on the same level with this project folder.
* Set environmental variables for PICO_SDK_PATH, PICO_EXTRAS_PATH and PICO_EXAMPLES_PATH
* Confirmed with Pico SDK 1.5.1
```
> git clone -b 1.5.1 https://github.com/raspberrypi/pico-sdk.git
> cd pico-sdk
> git submodule update -i
> cd ..
> git clone -b sdk-1.5.1 https://github.com/raspberrypi/pico-examples.git
>
> git clone -b sdk-1.5.1 https://github.com/raspberrypi/pico-extras.git
> 
> git clone -b main https://github.com/elehobica/pico_crp42602y_ctrl.git
> cd pico_crp42602y_ctrl
> git submodule update -i
> cd ..
### Windows
* Build is confirmed with Developer Command Prompt for VS 2022 and Visual Studio Code on Windows environment
* Confirmed with cmake-3.27.2-windows-x86_64 and gcc-arm-none-eabi-10.3-2021.10-win32
* Lanuch "Developer Command Prompt for VS 2022"
```
> cd pico_crp42602y_ctrl
> cd samples\xxxx
> mkdir build && cd build
> cmake -G "NMake Makefiles" ..
> nmake
```
* Put "xxxx.uf2" on RPI-RP2 drive
### Linux
* Build is confirmed with [rp2040-dev-docker:sdk-1.5.1-1.0.1]( https://hub.docker.com/r/elehobica/rp2040-dev-docker) 
* Confirmed with cmake-3.22.1 and arm-none-eabi-gcc (15:10.3-2021.07-4) 10.3.1
```
$ cd pico_crp42602y_ctrl
$ cd samples/xxxx
$ mkdir build && cd build
$ cmake ..
$ make -j4
```
* Download "xxxx.uf2" on RPI-RP2 drive
