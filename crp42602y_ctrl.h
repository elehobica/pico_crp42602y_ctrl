/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

#include "pico/stdlib.h"
#include "pico/util/queue.h"

class crp42602y_ctrl {
    private:
    // Definitions
    typedef enum _command_type_t {
        CMD_TYPE_NONE = 0,
        CMD_TYPE_STOP,
        CMD_TYPE_PLAY,
        CMD_TYPE_CUE
    } command_type_t;
    typedef enum _direction_t {
        DIR_KEEP = 0,  // relative FWD
        DIR_REVERSE,   // relative RWD
        DIR_FWD,       // absolute FWD (FWD for side A)
        DIR_RWD        // absolute RWD (FWD for side A)
    } direction_t;
    typedef struct _command_t {
        command_type_t type;
        direction_t    dir;
    } command_t;

    // Constants
    static constexpr int POWER_OFF_TIMEOUT_SEC = 300;
    static constexpr int COMMAND_QUEUE_LENGTH = 4;
    static constexpr int PERIODIC_FUNC_MS = 100;
    static constexpr int ROTATION_SENS_STOP_DETECT_MS = 1000;
    static constexpr int NUM_COMMAND_HISTORY_REGISTERED = 1;
    static constexpr int NUM_COMMAND_HISTORY_ISSUED = 2;
    // Internal command
    static constexpr command_t VOID_COMMAND         = {CMD_TYPE_NONE, DIR_KEEP};
    static constexpr command_t STOP_REVERSE_COMMAND = {CMD_TYPE_STOP, DIR_REVERSE};

    public:
    // Definitions
    typedef enum _reverse_mode_t {
        RVS_ONE_WAY = 0,
        RVS_ONE_ROUND,
        RVS_INFINITE_ROUND,
        __NUM_RVS_MODES__
    } reverse_mode_t;
    typedef enum _callback_type_t {
        ON_GEAR_ERROR = 0,
        ON_CASSETTE_SET,
        ON_CASSETTE_EJECT,
        ON_STOP,
        ON_REVERSE,
        ON_TIMEOUT_POWER_OFF,
        __NUM_CALLBACKS__
    } callback_type_t;

    // Constants
    // User commands
    static constexpr command_t STOP_COMMAND         = {CMD_TYPE_STOP, DIR_KEEP};
    static constexpr command_t PLAY_COMMAND         = {CMD_TYPE_PLAY, DIR_KEEP};
    static constexpr command_t PLAY_REVERSE_COMMAND = {CMD_TYPE_PLAY, DIR_REVERSE};
    static constexpr command_t PLAY_A_COMMAND       = {CMD_TYPE_PLAY, DIR_FWD};
    static constexpr command_t PLAY_B_COMMAND       = {CMD_TYPE_PLAY, DIR_RWD};
    static constexpr command_t FWD_COMMAND          = {CMD_TYPE_CUE,  DIR_FWD};
    static constexpr command_t RWD_COMMAND          = {CMD_TYPE_CUE,  DIR_RWD};

    crp42602y_ctrl(
        uint pin_cassette_detect,  // GPIO Input: Cassette detection
        uint pin_gear_status_sw,   // GPIO Input: Gear function status switch
        uint pin_rotation_sens,    // PWM_B Input: Rotation sensor
        uint pin_solenoid_ctrl,    // GPIO Output: This needs additional circuit to control solenoid
        uint pin_power_enable = 0, // GPIO Output: Power contrl (for timeout disable) (optional: 0 for not use)
        uint pin_rec_a_sw = 0,     // GPIO Input: Rec switch for A (optional: 0 for not use)
        uint pin_rec_b_sw = 0,     // GPIO Input: Rec switch for B (optional: 0 for not use)
        uint pin_type2_sw = 0      // GPIO Input: Type II switch (optional: 0 for not use)
        );
    virtual ~crp42602y_ctrl() {};
    void periodic_func_100ms();
    bool is_playing() const;
    bool is_cueing() const;
    bool set_head_dir_a(const bool head_dir_a);
    bool get_head_dir_a() const;
    void set_reverse_mode(const reverse_mode_t mode);
    reverse_mode_t get_reverse_mode() const;
    bool send_command(const command_t& command);
    void process_loop();
    void register_callback(const callback_type_t callback_type, void (*func)(const callback_type_t callback_type));
    void register_callback_all(void (*func)(const callback_type_t callback_type));

    private:
    uint _pin_cassette_detect;
    uint _pin_gear_status_sw;
    uint _pin_rotation_sens;
    uint _pin_solenoid_ctrl;
    uint _pin_power_ctrl;
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
    int _periodic_count;
    int _rot_stop_ignore_count;
    int _power_off_timeout_count;
    bool _has_cur_gear_status;
    bool _cur_head_dir_a;
    bool _cur_lift_head;
    bool _cur_reel_fwd;
    bool _power_enable;


    queue_t   _command_queue;
    command_t _command_history_registered[NUM_COMMAND_HISTORY_REGISTERED];
    command_t _command_history_issued[NUM_COMMAND_HISTORY_ISSUED];
    uint      _pwm_slice_num;
    uint16_t  _rot_count_history[ROTATION_SENS_STOP_DETECT_MS / PERIODIC_FUNC_MS];
    void (*_callbacks[__NUM_CALLBACKS__])(const callback_type_t callback_type);

    void _invoke_callback(const callback_type_t callback_type) const;
    void _pull_solenoid(const bool flag) const;
    void _set_power_enable(const bool flag);
    bool _get_power_enable() const;
    bool _is_gear_in_func() const;
    void _store_gear_status(const bool head_dir_a, const bool lift_head, const bool reel_fwd);
    bool _equal_gear_status(const bool head_dir_a, const bool lift_head, const bool reel_fwd) const;
    void _func_sequence(const bool head_dir_a, const bool lift_head, const bool reel_fwd);
    void _return_sequence() const;
    bool _get_abs_dir(const direction_t dir) const;
    void _stop() const;
    void _play(const direction_t dir);
    void _cue(const direction_t dir);
};