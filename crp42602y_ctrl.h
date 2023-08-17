/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

#include "pico/stdlib.h"
#include "pico/util/queue.h"

class crp42602y_ctrl {
    public:
    // Definitions
    typedef enum _command_type_t {
        CMD_TYPE_STOP = 0,
        CMD_TYPE_PLAY,
        CMD_TYPE_CUE
    } command_type_t;
    typedef enum _direction_t {
        DIR_KEEP = 0,  // relative FWD
        DIR_REVERSE,   // relative RWD
        DIR_FWD,       // absolute FWD (FWD for side A)
        DIR_RWD        // absolute RWD (FWD for side A)
    } direction_t;
    typedef enum _reverse_mode_t {
        RVS_ONE_WAY = 0,
        RVS_ONE_ROUND,
        RVS_INFINITE_ROUND
    } reverse_mode_t;
    typedef struct _command_t {
        command_type_t type;
        direction_t    dir;
    } command_t;

    // Constants
    static constexpr int COMMAND_QUEUE_LENGTH = 4;
    static constexpr int PERIODIC_FUNC_MS = 100;
    static constexpr int ROTATION_SENS_STOP_DETECT_MS = 1000;
    // User commands
    static constexpr command_t STOP_COMMAND   = {CMD_TYPE_STOP, DIR_KEEP};
    static constexpr command_t PLAY_A_COMMAND = {CMD_TYPE_PLAY, DIR_FWD};
    static constexpr command_t PLAY_B_COMMAND = {CMD_TYPE_PLAY, DIR_RWD};
    static constexpr command_t FWD_COMMAND    = {CMD_TYPE_CUE,  DIR_FWD};
    static constexpr command_t RWD_COMMAND    = {CMD_TYPE_CUE,  DIR_RWD};

    crp42602y_ctrl(
        uint pin_cassette_detect,  // GPIO Input: Cassette detection
        uint pin_gear_status_sw,   // GPIO Input: Gear function status switch
        uint pin_rotation_sens,    // PWM_B Input: Rotation sensor
        uint pin_solenoid_ctrl,    // GPIO Output: This needs additional circuit to control solenoid
        uint pin_rec_a_sw = 0,     // GPIO Input: Rec switch for A (optional: 0 for not use)
        uint pin_rec_b_sw = 0,     // GPIO Input: Rec switch for B (optional: 0 for not use)
        uint pin_type2_sw = 0      // GPIO Input: Type II switch (optional: 0 for not use)
        );
    virtual ~crp42602y_ctrl() {};
    void periodic_func_100ms();
    bool is_playing();
    bool is_cueing();
    bool is_dir_a();
    bool send_command(const command_t& command);
    void process_loop();

    private:
    uint _pin_cassette_detect;
    uint _pin_gear_status_sw;
    uint _pin_rotation_sens;
    uint _pin_solenoid_ctrl;
    uint _pin_rec_a_sw;
    uint _pin_rec_b_sw;
    uint _pin_type2_sw;
    uint _pin_pb_eq_ctrl;
    uint _pin_dolby_off_ctrl;
    uint _pin_dolby_c_ctrl;
    bool _head_dir_a;
    bool _has_cassette;
    reverse_mode_t _reverse_mode;
    bool _playing;
    bool _cueing;

    queue_t  _command_queue;
    uint     _pwm_slice_num;
    uint16_t _rot_count_history[ROTATION_SENS_STOP_DETECT_MS / PERIODIC_FUNC_MS];

    void _pull_solenoid(bool flag);
    bool _is_gear_in_func();
    void _func_sequence(bool head_dir_a, bool lift_head, bool reel_fwd);
    void _return_sequence();
    void _stop();
    bool _get_abs_dir(direction_t dir);
    void _play(direction_t dir);
    void _cue(direction_t dir);
};