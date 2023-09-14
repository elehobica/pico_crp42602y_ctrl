/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "crp42602y_counter.h"

class crp42602y_ctrl {
    protected:
    // Definitions
    typedef enum _command_type_t {
        CMD_TYPE_NONE = 0,
        CMD_TYPE_STOP,
        CMD_TYPE_PLAY,
        CMD_TYPE_CUE,
        __NUM_CMD_TYPE__
    } command_type_t;
    typedef enum _direction_t {
        DIR_KEEP = 0,  // relative Forward
        DIR_REVERSE,   // relative Backward
        DIR_FORWARD,   // absolute Forward (Forward for side A)
        DIR_BACKWARD   // absolute Backward (Forward for side B)
    } direction_t;
    typedef struct _command_t {
        command_type_t type;
        direction_t    dir;
    } command_t;
    typedef enum _filter_signal_t {
        FILT_CASSETTE_DETECT = 0,
        FILT_REC_A_OK,
        FILT_REC_B_OK,
        __NUM_FILTER_SIGNALS__
    } filter_signal_t;

    // Constants
    static constexpr uint32_t POWER_OFF_TIMEOUT_SEC = 300;
    static constexpr uint     COMMAND_QUEUE_LENGTH = 6;
    static constexpr uint     CALLBACK_QUEUE_LENGTH = 4;
    static constexpr uint32_t SIGNAL_FILTER_MS = 100;
    static constexpr uint32_t SIGNAL_FILTER_TIMES = 3;
    static constexpr int      NUM_COMMAND_HISTORY_REGISTERED = 1;
    static constexpr int      NUM_COMMAND_HISTORY_ISSUED = 2;
    // Internal commands
    static constexpr command_t VOID_COMMAND           = {CMD_TYPE_NONE, DIR_KEEP};
    static constexpr command_t STOP_REVERSE_COMMAND   = {CMD_TYPE_STOP, DIR_REVERSE};

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
        ON_COMMAND_FIFO_OVERFLOW,
        ON_CASSETTE_SET,
        ON_CASSETTE_EJECT,
        ON_STOP,
        ON_PLAY,
        ON_CUE,
        ON_REVERSE,
        ON_TIMEOUT_POWER_OFF,
        ON_RECOVER_POWER_FROM_TIMEOUT,
        __NUM_CALLBACK_TYPE__
    } callback_type_t;

    // Constants
    // User commands
    static constexpr command_t STOP_COMMAND         = {CMD_TYPE_STOP, DIR_KEEP};
    static constexpr command_t PLAY_COMMAND         = {CMD_TYPE_PLAY, DIR_KEEP};
    static constexpr command_t PLAY_REVERSE_COMMAND = {CMD_TYPE_PLAY, DIR_REVERSE};
    static constexpr command_t PLAY_A_COMMAND       = {CMD_TYPE_PLAY, DIR_FORWARD};
    static constexpr command_t PLAY_B_COMMAND       = {CMD_TYPE_PLAY, DIR_BACKWARD};
    static constexpr command_t FF_COMMAND           = {CMD_TYPE_CUE,  DIR_FORWARD};
    static constexpr command_t REW_COMMAND          = {CMD_TYPE_CUE,  DIR_BACKWARD};

    crp42602y_ctrl(
        const uint pin_cassette_detect,  // GPIO Input: Cassette detection
        const uint pin_gear_status_sw,   // GPIO Input: Gear function status switch
        const uint pin_rotation_sens,    // GPIO Input: Rotation sensor
        const uint pin_solenoid_ctrl,    // GPIO Output: This needs additional circuit to control solenoid
        const uint pin_power_enable = 0, // GPIO Output: Power contrl (for timeout disable) (optional: 0 for not use)
        const uint pin_rec_a_sw = 0,     // GPIO Input: Rec switch for A (optional: 0 for not use)
        const uint pin_rec_b_sw = 0      // GPIO Input: Rec switch for B (optional: 0 for not use)
    );
    virtual ~crp42602y_ctrl();
    bool is_playing() const;
    bool is_cueing() const;
    bool set_head_dir_is_a(const bool head_dir_is_a);
    bool get_head_dir_is_a() const;
    bool get_cue_dir_is_a() const;
    void set_reverse_mode(const reverse_mode_t mode);
    reverse_mode_t get_reverse_mode() const;
    void recover_power_from_timeout();
    bool send_command(const command_t& command);
    virtual void register_callback(const callback_type_t callback_type, void (*func)(const callback_type_t callback_type));
    virtual void register_callback_all(void (*func)(const callback_type_t callback_type));
    void on_rotation_stop();
    virtual void process_loop();

    protected:
    crp42602y_counter _counter;
    const uint _pin_cassette_detect;
    const uint _pin_gear_status_sw;
    const uint _pin_solenoid_ctrl;
    const uint _pin_power_ctrl;
    const uint _pin_rec_a_sw;
    const uint _pin_rec_b_sw;
    bool _head_dir_is_a;
    bool _cue_dir_is_a;
    bool _has_cassette;
    bool _rec_a_ok;
    bool _rec_b_ok;
    bool _prev_has_cassette;
    reverse_mode_t _reverse_mode;
    bool _playing;
    bool _cueing;
    uint32_t _prev_filter_time;
    uint32_t _prev_func_time;
    bool _has_cur_gear_status;
    bool _cur_head_dir_is_a;
    bool _cur_lift_head;
    bool _cur_reel_fwd;
    bool _gear_changing;
    uint32_t _gear_last_time;
    bool _power_enable;
    uint32_t _signal_filter[__NUM_FILTER_SIGNALS__];

    command_t _command_history_registered[NUM_COMMAND_HISTORY_REGISTERED];
    command_t _command_history_issued[NUM_COMMAND_HISTORY_ISSUED];
    void (*_callbacks[__NUM_CALLBACK_TYPE__])(const callback_type_t callback_type);
    queue_t   _command_queue;
    queue_t   _callback_queue;

    void _gpio_callback(uint gpio, uint32_t events);
    void _filter_signal(const filter_signal_t filter_signal, const bool raw_signal, bool& filtered_signal);
    bool _dispatch_callback(const callback_type_t callback_type);
    void _set_power_enable(const bool flag);
    bool _get_power_enable() const;
    void _pull_solenoid(const bool flag) const;
    virtual bool _is_playing_internal() const;
    bool _gear_is_changing() const;
    bool _gear_is_in_func() const;
    void _gear_store_status(const bool head_dir_is_a, const bool lift_head, const bool reel_fwd);
    bool _gear_is_equal_status(const bool head_dir_is_a, const bool lift_head, const bool reel_fwd) const;
    bool _gear_func_sequence(const bool head_dir_is_a, const bool lift_head, const bool reel_fwd);
    bool _gear_return_sequence();
    bool _get_dir_is_a(const direction_t dir) const;
    bool _stop(const direction_t dir);
    bool _play(const direction_t dir);
    bool _cue(const direction_t dir);
    virtual void _process_filter(uint32_t now);
    virtual void _process_set_eject_detection();
    virtual void _process_timeout_power_off(uint32_t now);
    virtual void _process_command();
    virtual void _process_callbacks();

    friend crp42602y_counter;
};

class crp42602y_ctrl_with_counter : public crp42602y_ctrl {
    protected:
    // Definitions
    typedef enum _command_type_extend_t {
        CMD_TYPE_WAIT = __NUM_CMD_TYPE__,
        CMD_TYPE_HEAD_DIR,
        __NUM_CMD_TYPE_EXTEND__
    } command_type_extend_t;
    // Internal commands
    static constexpr command_t WAIT_FF_READY_COMMAND  = {(command_type_t) CMD_TYPE_WAIT, DIR_FORWARD};
    static constexpr command_t WAIT_REW_READY_COMMAND = {(command_type_t) CMD_TYPE_WAIT, DIR_BACKWARD};
    static constexpr command_t HEAD_DIR_A_COMMAND     = {(command_type_t) CMD_TYPE_HEAD_DIR, DIR_FORWARD};
    static constexpr command_t HEAD_DIR_B_COMMAND     = {(command_type_t) CMD_TYPE_HEAD_DIR, DIR_BACKWARD};

    public:
    typedef enum _callback_type_extend_t {
        ON_COUNTER_FIFO_OVERFLOW = __NUM_CALLBACK_TYPE__,
        __NUM_CALLBACK_TYPE_EXTEND__
    } callback_type_extend_t;
    crp42602y_ctrl_with_counter (
        const uint pin_cassette_detect,  // GPIO Input: Cassette detection
        const uint pin_gear_status_sw,   // GPIO Input: Gear function status switch
        const uint pin_rotation_sens,    // GPIO Input: Rotation sensor
        const uint pin_solenoid_ctrl,    // GPIO Output: This needs additional circuit to control solenoid
        const uint pin_power_enable = 0, // GPIO Output: Power contrl (for timeout disable) (optional: 0 for not use)
        const uint pin_rec_a_sw = 0,     // GPIO Input: Rec switch for A (optional: 0 for not use)
        const uint pin_rec_b_sw = 0      // GPIO Input: Rec switch for B (optional: 0 for not use)
    );
    virtual ~crp42602y_ctrl_with_counter();
    crp42602y_counter* get_counter_inst();
    virtual void register_callback(const callback_type_t callback_type, void (*func)(const callback_type_t callback_type));
    virtual void register_callback_all(void (*func)(const callback_type_t callback_type));
    virtual void process_loop();

    protected:
    bool _playing_for_wait_cue;

    void (*_callbacks[__NUM_CALLBACK_TYPE_EXTEND__ - __NUM_CALLBACK_TYPE__])(const callback_type_t callback_type);

    bool _is_playing_internal() const;
    bool _is_que_ready_for_counter(direction_t dir) const;
    virtual void _process_set_eject_detection();
    virtual void _process_command();
    virtual void _process_callbacks();
};