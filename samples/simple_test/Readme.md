# Raspberry Pi Pico CRP42602Y mechanism control

## Simple test project
* test for single deck (play back only)

## Supported Board and Peripheral Devices
* Raspberry Pi Pico (rp2040)
* CRP42602Y mechanism

## Pin Assignment & Connection
### CRP42602 connector
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 5 | GP3 | GPIO Input | CASSETTE_DETECT from CRP42602Y pin 7 |
| 6 | GP4 | GPIO Input | GEAR_STATUS_SW from CRP42602Y pin 3 |
| 7 | GP5 | PWM_B Input | ROTATION_SENS from CRP42602Y pin 2 |
| 8 | GND | GND | GND |

### CRP42602 control with additional circuit
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 4 | GP2 | GPIO Output | to SOLENOID_CTRL (0: release, 1: pull) |
| 8 | GND | GND | GND |
| 9 | GP6 | GPIO Output | to POWER_CTRL (0: disable, 1: enable) |

## Serial interface operation
* 's': stop
* 'p': play
* 'q': reverse play
* 'f': fast forward
* 'r': rewind
* 'd': direction A/B
* 'v': reverse mode