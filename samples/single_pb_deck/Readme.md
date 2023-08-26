# Raspberry Pi Pico CRP42602Y mechanism control

## Single PB deck project
* support single deck (play back only)
* control by 5 Way Switch + 2 Buttons and serial interface
* support EQ and NR control

## Supported Board and Peripheral Devices
* Raspberry Pi Pico (rp2040)
* CRP42602Y mechanism
* TA7668BP + CXA1332M board with customization
* SSD1306 128x64 OLED display

## Pin Assignment & Connection
### Raspberry Pi Pico
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 4 | GP2 | GPIO Output | to SOLENOID_CTRL of additional circuit (0: release, 1: pull) |
| 5 | GP3 | GPIO Input | CASSETTE_DETECT from CRP42602Y pin 7 |
| 6 | GP4 | GPIO Input | GEAR_STATUS_SW from CRP42602Y pin 3 |
| 7 | GP5 | PWM_B Input | ROTATION_SENS from CRP42602Y pin 2 |
| 8 | GND | GND | GND |
| 9 | GP6 | GPIO Output | to POWER_CTRL (0: disable, 1: enable) |

### EQ and NR control with additional circuit
| Pico Pin # | GPIO | Function | Connection |
----|----|----|----
| 10 | GP7 | GPIO Output | to EQ_CTRL (0: 120us, 1: 70us) |
| 13 | GND | GND | GND |
| 14 | GP10 | GPIO Output | to NR_CTRL0 (0: Dolby B, 1: Dolby C) |
| 15 | GP11 | GPIO Output | to NR_CTRL1 (0: NR ON, 1: NR OFF) |

### SSD1306
| Pico Pin # | Pin Name | Function | Connection |
----|----|----|----
| 11 | GP8 | I2C0 SDA | to/from SSD1306 SDA |
| 12 | GP9 | I2C0 SCL | to/from SSD1306 SDA |

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
* 's': stop
* 'p': play
* 'q': reverse play
* 'f': fast forward
* 'r': rewind
* 'd': direction A/B
* 'v': reverse mode
* 'e': EQ select
* 'n': NR select