# Raspberry Pi Pico CRP42602Y mechanism control

## Single deck project
* support single deck
* control by 5 Way Switch + 2 Buttons and serial interface
* support EQ and NR control for pre-amp

## Pin Assignment & Connection
### CRP42602 Connector
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 4 | GP2 | SOLENOID_CTRL | to solenoid control (0: pull, 1: release) |
| 5 | GP3 | CASSETTE_DETECT | from CRP42602Y pin 7 |
| 6 | GP4 | FUNC_STATUS_SW | from CRP42602Y pin 3 |
| 7 | GP5 | ROTATION_SENS | from CRP42602Y pin 2 |
| 8 | GND | GND | GND |
| 9 | GP6 | POWER_CTRL | to power control (0: disable, 1: enable) |

### 5 Way Switch + 2 Buttons
| Pico Pin # | Pin Name | Function | Connection |
----|----|----|----
| 23 | GND | GND | COM |
| 24 | GP18 | GPIO Input | UP |
| 25 | GP19 | GPIO Input | DOWN |
| 26 | GP20 | GPIO Input | LEFT |
| 27 | GP21 | GPIO Input | RIGHT |
| 29 | GP22 | GPIO Input | CENTER |
| 31 | GP26 | GPIO Input | SET |
| 32 | GP27 | GPIO Input | RESET |

## Button Operation
* center: play/stop (single click), reverse play (double click)
* down: fwd
* up: rwd
* right: direction A/B
* left: reverse mode
* reset: EQ select
* set: NR select
