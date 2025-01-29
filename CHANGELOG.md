# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]
### Added
* Add is_operating()
* Add ON_FF_REW callback
* Add robustness for gear error (add timeout and ignore)
* Add EQ mute control for single_pb_deck project
* Add Bluetooth Tx control for single_pb_deck project
* Support inverting head direction on stop depending on reverse mode
* Add Discrete EQ plus CXA1332M board schematic
### Changed
* Support pico-sdk 2.1.0
* Revise TA7668BP_CXA1332M_board_customized schematic to improve low frequency gain
* Set motor off at initial without cassette
* Integrate single_pb_deck_with_rt_counter project into single_pb_deck project
* Change power recovery behavior when any buttons or serial commands during power off for single_pb_deck project
### Fixed
* Ignore commands except for STOP when no cassette
* Introduce _stop_queue to remove _command_queue safely
* Correct real-time counter auto reset case (at auto-reverse timing only)

## [0.8.0] - 2024-06-02
### Added
* Calculate hub rotation by PIO, not PWM
* Support early stop with PIO state machine
* Support real-time counter on single_pb_deck_with_rt_counter project
* Add the schematics for EQ and NR control
### Changed
* Support pico-sdk 1.5.1 (previously 1.4.0)
* Change library name to pico_crp42602y_ctrl (previously crp42602y_ctrl)

## [0.7.0] - 2023-08-28
* Initial release