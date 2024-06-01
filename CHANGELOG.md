# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]
### Added
* Calculate rotation by PIO, not PWM
* Support early stop with PIO state machine
* Support real-time counter
  * Add single_pb_deck_with_rt_counter project
  * Add C-120 thickness estimation in addition to C-46 (C-60), C-90 and C-100
  * Support tape thickness estimation
* Add the schematics for EQ and NR control
### Changed
* Support pico-sdk 1.5.1 (previously 1.4.0)

## [0.7.0] - 2023-08-28
* Initial release