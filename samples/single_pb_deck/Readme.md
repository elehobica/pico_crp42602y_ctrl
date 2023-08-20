# Raspberry Pi Pico CRP42602Y mechanism control

## Single PB deck project
* support single deck (play back only)
* control by 5 Way Switch + 2 Buttons and serial interface
* support EQ and NR control for pre-amp

## Pin Assignment & Connection
### CRP42602 connector
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 5 | GP3 | CASSETTE_DETECT | from CRP42602Y pin 7 |
| 6 | GP4 | FUNC_STATUS_SW | from CRP42602Y pin 3 |
| 7 | GP5 | ROTATION_SENS | from CRP42602Y pin 2 |
| 8 | GND | GND | GND |

### CRP42602 control with additional circuit
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 4 | GP2 | SOLENOID_CTRL | to solenoid control (mandatory) (0: pull, 1: release) |
| 8 | GND | GND | GND |
| 9 | GP6 | POWER_CTRL | to power control (optional) (0: disable, 1: enable) |

### EQ and NR control with additional circuit
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 10 | GP7 | EQ_CTRL | to EQ control (0: 120us, 1: 70us) |
| 13 | GND | GND | GND |
| 14 | GP10 | NR_CTRL0 | to NR control (0: Dolby B, 1: Dolby C) |
| 15 | GP11 | NR_CTRL1 | to NR control (0: NR ON, 1: NR OFF) |

### 5 Way switch + 2 buttons
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

## Button operation
* center: play/stop (single click), reverse play (double click)
* down: fast forward
* up: rewind
* right: direction A/B
* left: reverse mode
* reset: EQ select
* set: NR select

## Serial interface operation
* 's': play
* 'p': play
* 'q': reverse play
* 'f': fast forward
* 'r': rewind
* 'd': direction A/B
* 'v': reverse mode
* 'e': EQ select
* 'n': NR select