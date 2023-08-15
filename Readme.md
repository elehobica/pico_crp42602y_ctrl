# Raspberry Pi Pico CRP42602Y mechanism control

## Overview
* Control firmware for CRP42602Y auto-reverse cassette tape deck mechanism
* Generate solenoid control timing for function gear of CRP42602Y 

## Supported Board and Peripheral Devices
* Raspberry Pi Pico (rp2040)
* CRP42602Y mechanism

## Pin Assignment & Connection
### CRP42602 Connector
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 4 | GP2 | SOLENOID_CTRL | to solenoid control (0: pull, 1: release) |
| 5 | GP3 | CASSETTE_DETECT | from CRP42602Y pin 7 |
| 6 | GP4 | FUNC_STATUS_SW | from CRP42602Y pin 3 |
| 7 | GP5 | ROTATION_SENS | from CRP42602Y pin 2 |
| 8 | GND | GND | GND |

### 5 Way Switch + 2 Buttons
| Pico Pin # | Pin Name | Function | Connection |
----|----|----|----
| 23 | GND | GND | COM |
| 24 | GP18 | GPIO Input | DOWN |
| 25 | GP19 | GPIO Input | UP |
| 26 | GP20 | GPIO Input | LEFT |
| 27 | GP21 | GPIO Input | RIGHT |
| 29 | GP22 | GPIO Input | CENTER |
| 31 | GP26 | GPIO Input | SET |
| 32 | GP27 | GPIO Input | RESET |

## How to build
* See ["Getting started with Raspberry Pi Pico"](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
* Build is confirmed only in Developer Command Prompt for VS 2019 and Visual Studio Code on Windows enviroment
* Put "pico-sdk", "pico-examples" (, "pico-extras" and "pico-playground") on the same level with this project folder.
* Confirmed under Pico SDK 1.4.0
```
> git clone -b master https://github.com/raspberrypi/pico-sdk.git
> cd pico-sdk
> git submodule update -i
> cd ..
> git clone -b master https://github.com/raspberrypi/pico-examples.git
>
> git clone https://github.com/raspberrypi/pico-extras.git
> cd pico-extras
> git submodule update -i
> cd ..
> 
> git clone -b main https://github.com/elehobica/pico_crp42602y_ctrl.git
```
* Lanuch "Developer Command Prompt for VS 2019"
```
> cd pico_crp42602y_ctrl
> git submodule update -i
> mkdir build
> cd build
> cmake -G "NMake Makefiles" ..
> nmake
```
* Put "pico_crp42602y_ctrl.uf2" on RPI-RP2 drive