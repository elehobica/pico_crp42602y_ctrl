/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "crp42602y_ctrl.h"

#include <cstdio>

static inline uint32_t _millis()
{
    return to_ms_since_boot(get_absolute_time());
}

static inline uint32_t _get_diff_time(uint32_t prev, uint32_t now)
{
    uint32_t diff_time;
    if (now < prev) {
        diff_time = 0xffffffffUL - prev + now + 1;
    } else {
        diff_time = now - prev;
    }
    return diff_time;
}

crp42602y_ctrl::crp42602y_ctrl(
    const uint pin_cassette_detect,
    const uint pin_gear_status_sw,
    const uint pin_rotation_sens,
    const uint pin_solenoid_ctrl,
    const uint pin_power_ctrl,
    const uint pin_rec_a_sw,
    const uint pin_rec_b_sw
) :
    _counter(pin_rotation_sens, this),
    _pin_cassette_detect(pin_cassette_detect),
    _pin_gear_status_sw(pin_gear_status_sw),
    _pin_solenoid_ctrl(pin_solenoid_ctrl),
    _pin_power_ctrl(pin_power_ctrl),
    _pin_rec_a_sw(pin_rec_a_sw),
    _pin_rec_b_sw(pin_rec_b_sw),
    _head_dir_is_a(true),
    _cue_dir_is_a(true),
    _has_cassette(false),
    _rec_a_ok(false),
    _rec_b_ok(false),
    _prev_has_cassette(false),
    _reverse_mode(RVS_ONE_ROUND),
    _playing(false),
    _cueing(false),
    _prev_filter_time(0),
    _prev_func_time(0),
    _has_cur_gear_status(false),
    _cur_head_dir_is_a(false),
    _cur_lift_head(false),
    _cur_reel_fwd(false),
    _gear_changing(false),
    _gear_last_time(0),
    _power_enable(true),
    _signal_filter{}
{
    for (int i = 0; i < NUM_COMMAND_HISTORY_REGISTERED; i++) {
        _command_history_registered[i] = VOID_COMMAND;
    }
    for (int i = 0; i < NUM_COMMAND_HISTORY_ISSUED; i++) {
        _command_history_issued[i] = VOID_COMMAND;
    }
    for (int i = 0; i < __NUM_CALLBACK_TYPE__; i++) {
        _callbacks[i] = nullptr;
    }

    queue_init(&_command_queue, sizeof(command_t), COMMAND_QUEUE_LENGTH);
    queue_init(&_callback_queue, sizeof(callback_type_t), CALLBACK_QUEUE_LENGTH);

    // GPIO setting (pull-up should be done in advance outside if needed)
    gpio_init(_pin_cassette_detect);
    gpio_set_dir(_pin_cassette_detect, GPIO_IN);

    gpio_init(_pin_gear_status_sw);
    gpio_set_dir(_pin_gear_status_sw, GPIO_IN);

    gpio_init(_pin_solenoid_ctrl);
    _pull_solenoid(false); // set default before setting output mode
    gpio_set_dir(_pin_solenoid_ctrl, GPIO_OUT);

    if (_pin_power_ctrl != 0) {
        gpio_init(_pin_power_ctrl);
        _set_power_enable(true); // set default before setting output mode
        gpio_set_dir(_pin_power_ctrl, GPIO_OUT);
    }
}

crp42602y_ctrl::~crp42602y_ctrl()
{
    queue_free(&_command_queue);
    queue_free(&_callback_queue);
}

bool crp42602y_ctrl::is_playing() const
{
    return _playing;
}

bool crp42602y_ctrl::is_cueing() const
{
    return _cueing;
}

bool crp42602y_ctrl::set_head_dir_is_a(const bool head_dir_is_a)
{
    // ignore when in func
    if (!_gear_is_in_func()) {
        _head_dir_is_a = head_dir_is_a;
    }
    return _head_dir_is_a;
}

bool crp42602y_ctrl::get_head_dir_is_a() const
{
    return _head_dir_is_a;
}

bool crp42602y_ctrl::get_cue_dir_is_a() const
{
    return _cue_dir_is_a;
}

void crp42602y_ctrl::set_reverse_mode(const reverse_mode_t mode)
{
    _reverse_mode = mode;
}

crp42602y_ctrl::reverse_mode_t crp42602y_ctrl::get_reverse_mode() const
{
    return _reverse_mode;
}

void crp42602y_ctrl::recover_power_from_timeout()
{
    bool power_enable = _power_enable;
    _set_power_enable(true);
    if (!power_enable) {
        _dispatch_callback(ON_RECOVER_POWER_FROM_TIMEOUT);
    }
}

bool crp42602y_ctrl::send_command(const command_t& command)
{
    // Cancel same repeated command except for DIR_REVERSE
    if (_command_history_registered[0].type == command.type && _command_history_registered[0].dir == command.dir && command.dir != DIR_REVERSE)
        return false;

    if (command.type == CMD_TYPE_STOP) {
        while (!queue_is_empty(&_command_queue)) {
            command_t remove_command;
            queue_remove_blocking(&_command_queue, &remove_command);
        }
    }

    if (queue_try_add(&_command_queue, &command)) {
        for (int i = NUM_COMMAND_HISTORY_REGISTERED - 1; i >= 1; i--) {
            _command_history_registered[i] = _command_history_registered[i - 1];
        }
        _command_history_registered[0] = command;
        return true;
    } else {
        _dispatch_callback(ON_COMMAND_FIFO_OVERFLOW);
        return false;
    }
}

void crp42602y_ctrl::register_callback(const callback_type_t callback_type, void (*func)(const callback_type_t callback_type))
{
    _callbacks[callback_type] = func;
}

void crp42602y_ctrl::register_callback_all(void (*func)(const callback_type_t callback_type))
{
    for (int i = 0; i < __NUM_CALLBACK_TYPE__; i++) {
        register_callback((const callback_type_t) i, func);
    }
}

void crp42602y_ctrl::on_rotation_stop()
{
    if (!_gear_is_in_func()) return;
    uint32_t now = _millis();
    if (now < _gear_last_time + 1000) return;

    // reverse if previous command is play, or cue after play in same direction
    bool reverse_flag = _command_history_issued[0].type == CMD_TYPE_PLAY  ||
                        (_command_history_issued[0].type == CMD_TYPE_CUE && _command_history_issued[1].type == CMD_TYPE_PLAY &&
                            (_command_history_issued[0].dir == DIR_FORWARD) == _head_dir_is_a);
    switch (_reverse_mode) {
    case RVS_ONE_WAY:
        send_command(STOP_COMMAND);
        break;
    case RVS_ONE_ROUND:
        if (reverse_flag) {
            if (_head_dir_is_a) {
                send_command(PLAY_REVERSE_COMMAND);
            } else {
                // It is expected to play A at next time after one round stops
                send_command(STOP_REVERSE_COMMAND);
            }
        } else {
            send_command(STOP_COMMAND);
        }
        break;
    case RVS_INFINITE_ROUND:
        if (reverse_flag) {
            send_command(PLAY_REVERSE_COMMAND);
        } else {
            send_command(STOP_COMMAND);
        }
        break;
    default:
        send_command(STOP_COMMAND);
        break;
    }
}

void crp42602y_ctrl::process_loop()
{
    uint32_t now = _millis();
    _process_filter(now);
    _process_set_eject_detection();
    _process_timeout_power_off(now);
    _process_command();
    _process_callbacks();
}

void crp42602y_ctrl::_filter_signal(const filter_signal_t filter_signal, const bool raw_signal, bool& filtered_signal)
{
    // Shift
    _signal_filter[filter_signal] = (_signal_filter[filter_signal] << 1) | raw_signal;

    // Apply if same value repeated SIGNAL_FILTER_TIMES times
    uint32_t mask = (1U << SIGNAL_FILTER_TIMES) - 1;
    if ((_signal_filter[filter_signal] & mask) == mask) {
        filtered_signal = true;
    } else if ((_signal_filter[filter_signal] & mask) == 0) {
        filtered_signal = false;
    }
}

bool crp42602y_ctrl::_dispatch_callback(const callback_type_t callback_type)
{
    return queue_try_add(&_callback_queue, &callback_type);
}

void crp42602y_ctrl::_set_power_enable(const bool flag)
{
    if (_pin_power_ctrl != 0) {
        gpio_put(_pin_power_ctrl, flag);
        _power_enable = flag;
    }
}

bool crp42602y_ctrl::_get_power_enable() const
{
    return _power_enable;
}

void crp42602y_ctrl::_pull_solenoid(const bool flag) const
{
    gpio_put(_pin_solenoid_ctrl, flag);
}

bool crp42602y_ctrl::_is_playing_internal() const
{
    return _playing;
}

bool crp42602y_ctrl::_gear_is_changing() const
{
    return _gear_changing;
}

bool crp42602y_ctrl::_gear_is_in_func() const
{
    return !gpio_get(_pin_gear_status_sw) && _power_enable;
}

void crp42602y_ctrl::_gear_store_status(const bool head_dir_is_a, const bool lift_head, const bool reel_fwd)
{
    _has_cur_gear_status = true;
    _cur_head_dir_is_a = head_dir_is_a;
    _cur_lift_head = lift_head;
    _cur_reel_fwd = reel_fwd;
}

bool crp42602y_ctrl::_gear_is_equal_status(const bool head_dir_is_a, const bool lift_head, const bool reel_fwd) const
{
    return _has_cur_gear_status == true &&
        _cur_head_dir_is_a == head_dir_is_a && _cur_lift_head == lift_head && _cur_reel_fwd == reel_fwd;
}

bool crp42602y_ctrl::_gear_func_sequence(const bool head_dir_is_a, const bool lift_head, const bool reel_fwd)
{
    constexpr uint32_t tWaitMotorStable = 500;

    // recover power if disabled
    if (!_power_enable) {
        recover_power_from_timeout();
        sleep_ms(tWaitMotorStable);
    }

    _gear_last_time = _millis();

    // Function sequence has 190 degree of function gear to rotate in 400 ms
    // Timing definitions (milliseconds) (All values are set experimentally)
    constexpr uint32_t tInitS     = 0;           // Unhook the function gear
    constexpr uint32_t tInitE     = 20;
    constexpr uint32_t tHeadDirS  = tInitE;      // Term to determine direction of head and pinch roller
    constexpr uint32_t tHeadDirE  = 100;
    constexpr uint32_t tLiftHeadS = 150;         // Term to lift head / evacuate head
    constexpr uint32_t tLiftHeadE = 300;
    constexpr uint32_t tReelS     = tLiftHeadE;  // Term to determine reel direction
    constexpr uint32_t tReelE     = 400;

    // Be careful about the consistency of pinch roller direction and reel direction,
    //  otherwise they could pull to opposite directions and give unexpected extension stress to the tape

    _pull_solenoid(true);
    sleep_ms(tInitE);
    _pull_solenoid(!head_dir_is_a);
    sleep_ms(tHeadDirE - tInitE);
    _pull_solenoid(false);
    sleep_ms(tLiftHeadS - tHeadDirE);
    _pull_solenoid(lift_head);
    sleep_ms(tLiftHeadE - tLiftHeadS);
    _pull_solenoid(reel_fwd);
    sleep_ms(tReelE - tLiftHeadE);
    _pull_solenoid(false);
    sleep_ms(20);  // additional margin

    if (!_gear_is_in_func()) {
        _dispatch_callback(ON_GEAR_ERROR);
        return false;
    }

    _gear_store_status(head_dir_is_a, lift_head, reel_fwd);
    return true;
}

bool crp42602y_ctrl::_gear_return_sequence()
{
    _gear_last_time = _millis();

    // Return sequence has (360 - 190) degree of function gear,
    //  which is needed to take another function when the gear is already in function position
    //  it is supposed to take 360 ms
    _pull_solenoid(true);
    sleep_ms(20);
    _pull_solenoid(false);
    sleep_ms(340);
    sleep_ms(20);  // additional margin

    if (_gear_is_in_func()) {
        _dispatch_callback(ON_GEAR_ERROR);
        return false;
    }

    return true;
}

bool crp42602y_ctrl::_get_dir_is_a(direction_t dir) const
{
    bool dir_is_a;
    switch(dir) {
    case DIR_KEEP:
        dir_is_a = _head_dir_is_a;
        break;
    case DIR_REVERSE:
        dir_is_a = !_head_dir_is_a;
        break;
    case DIR_FORWARD:
        dir_is_a = true;
        break;
    case DIR_BACKWARD:
        dir_is_a = false;
        break;
    default:
        dir_is_a = _head_dir_is_a;
        break;
    }
    return dir_is_a;
}

bool crp42602y_ctrl::_stop(direction_t dir)
{
    if (_gear_is_in_func()) {
        _gear_changing = true;
        if (!_gear_return_sequence()) {
            _gear_changing = false;
            return false;
        }
    }
    if (dir == DIR_REVERSE) _head_dir_is_a = !_head_dir_is_a;
    _gear_changing = false;
    return true;
}

bool crp42602y_ctrl::_play(direction_t dir)
{
    _head_dir_is_a = _get_dir_is_a(dir);
    if (_gear_is_in_func()) {
        if (_gear_is_equal_status(_head_dir_is_a, true, _head_dir_is_a)) return false;
        _gear_changing = true;
        if (!_gear_return_sequence()) {
            _gear_changing = false;
            return false;
        }
    }
    if (!_has_cassette) {
        _gear_changing = false;
        return false;
    }
    _gear_changing = true;
    bool flag = _gear_func_sequence(_head_dir_is_a, true, _head_dir_is_a);
    _gear_changing = false;
    return flag;
}

bool crp42602y_ctrl::_cue(direction_t dir)
{
    _cue_dir_is_a = _get_dir_is_a(dir);
    if (_gear_is_in_func()) {
        if (_gear_is_equal_status(_head_dir_is_a, false, _cue_dir_is_a)) return false;
        _gear_changing = true;
        if (!_gear_return_sequence()) {
            _gear_changing = false;
            return false;
        }
    }
    if (!_has_cassette) {
        _gear_changing = false;
        return false;
    }
    _gear_changing = true;
    // Evacuate head, however note that the head direction still matters for which side the head is tracing,
    bool flag = _gear_func_sequence(_head_dir_is_a, false, _cue_dir_is_a);
    _gear_changing = false;
    return flag;
}

void crp42602y_ctrl::_process_filter(uint32_t now)
{
    if (_get_diff_time(_prev_filter_time, now) >= SIGNAL_FILTER_MS) {
        // Switch filters
        _filter_signal(FILT_CASSETTE_DETECT, !gpio_get(_pin_cassette_detect), _has_cassette);
        _filter_signal(FILT_REC_A_OK, ((_pin_rec_a_sw != 0) ? !gpio_get(_pin_rec_a_sw) : 0), _rec_a_ok);
        _filter_signal(FILT_REC_B_OK, ((_pin_rec_b_sw != 0) ? !gpio_get(_pin_rec_a_sw) : 0), _rec_b_ok);
        _prev_filter_time = now;
    }
}

void crp42602y_ctrl::_process_set_eject_detection()
{
    // Cassette set/eject detection
    if (!_prev_has_cassette && _has_cassette) {
        _dispatch_callback(ON_CASSETTE_SET);
    } else if (_prev_has_cassette && !_has_cassette) {
        _dispatch_callback(ON_CASSETTE_EJECT);
        if (_gear_is_in_func()) {
            send_command(STOP_COMMAND);
        }
    }
    _prev_has_cassette = _has_cassette;
}

void crp42602y_ctrl::_process_timeout_power_off(uint32_t now)
{
    // Timeout power off (for mechanism)
    if (_gear_is_in_func() || !_get_power_enable() || _pin_power_ctrl == 0) {
        _prev_func_time = now;
    }
    if (_get_diff_time(_prev_func_time, now) >= POWER_OFF_TIMEOUT_SEC * 1000 && _get_power_enable()) {
        _set_power_enable(false);
        _dispatch_callback(ON_TIMEOUT_POWER_OFF);
    }
}

void crp42602y_ctrl::_process_command()
{
    // Process command
    if (queue_get_level(&_command_queue) > 0) {
        command_t command;
        queue_remove_blocking(&_command_queue, &command);
        switch (command.type) {
        case CMD_TYPE_STOP:
            if (_stop(command.dir)) {
                _playing = false;
                _cueing = false;
                _dispatch_callback(ON_STOP);
            }
            break;
        case CMD_TYPE_PLAY:
            if (_play(command.dir)) {
                _playing = true;
                _cueing = false;
                if (command.dir == DIR_REVERSE) {
                    _dispatch_callback(ON_REVERSE);
                } else {
                    _dispatch_callback(ON_PLAY);
                }
            }
            break;
        case CMD_TYPE_CUE:
            if (_cue(command.dir)) {
                _playing = false;
                _cueing = true;
                _dispatch_callback(ON_CUE);
            }
            break;
        default:
            break;
        }
        for (int i = NUM_COMMAND_HISTORY_ISSUED - 1; i >= 1; i--) {
            _command_history_issued[i] = _command_history_issued[i - 1];
        }
        _command_history_issued[0] = command;
    }
}

void crp42602y_ctrl::_process_callbacks()
{
    // Process callback
    while (queue_get_level(&_callback_queue) > 0) {
        callback_type_t callback_type;
        queue_remove_blocking(&_callback_queue, &callback_type);
        if (_callbacks[callback_type] != nullptr) {
            _callbacks[callback_type](callback_type);
        }
    }
}

// -------------------------------------------------------------------------------

crp42602y_ctrl_with_counter::crp42602y_ctrl_with_counter(
    const uint pin_cassette_detect,
    const uint pin_gear_status_sw,
    const uint pin_rotation_sens,
    const uint pin_solenoid_ctrl,
    const uint pin_power_ctrl,
    const uint pin_rec_a_sw,
    const uint pin_rec_b_sw
) :
    crp42602y_ctrl(pin_cassette_detect, pin_gear_status_sw, pin_rotation_sens, pin_solenoid_ctrl, pin_power_ctrl, pin_rec_a_sw, pin_rec_b_sw), 
    _playing_for_wait_cue(false)
{
    for (int i = 0; i < __NUM_CALLBACK_TYPE_EXTEND__ - __NUM_CALLBACK_TYPE__; i++) {
        _callbacks[i] = nullptr;
    }
    _counter.enable_counter();
}

crp42602y_ctrl_with_counter::~crp42602y_ctrl_with_counter()
{
}

crp42602y_counter* crp42602y_ctrl_with_counter::get_counter_inst()
{
    return &_counter;
}

void crp42602y_ctrl_with_counter::register_callback(const callback_type_t callback_type, void (*func)(const callback_type_t callback_type))
{
    if (callback_type >= __NUM_CALLBACK_TYPE__) {
        _callbacks[callback_type - __NUM_CALLBACK_TYPE__] = func;
    } else {
        crp42602y_ctrl::register_callback(callback_type, func);
    }
}

void crp42602y_ctrl_with_counter::register_callback_all(void (*func)(const callback_type_t callback_type))
{
    for (int i = 0; i < __NUM_CALLBACK_TYPE_EXTEND__; i++) {
        register_callback((const callback_type_t) i, func);
    }
}

void crp42602y_ctrl_with_counter::process_loop()
{
    uint32_t now = _millis();
    _process_filter(now);
    _process_set_eject_detection();
    _process_timeout_power_off(now);
    _process_command();
    _process_callbacks();
    _counter._process();
}

bool crp42602y_ctrl_with_counter::_is_playing_internal() const
{
    return _playing_for_wait_cue;
}

bool crp42602y_ctrl_with_counter::_is_que_ready_for_counter(direction_t dir) const
{
    int fs = (int) !_get_dir_is_a(dir);
    return _counter._check_status(crp42602y_counter::RADIUS_A_BIT << fs);
}

void crp42602y_ctrl_with_counter::_process_set_eject_detection()
{
    // Cassette set/eject detection
    if (!_prev_has_cassette && _has_cassette) {
        _counter.restart();
        _dispatch_callback(ON_CASSETTE_SET);
    } else if (_prev_has_cassette && !_has_cassette) {
        _counter.restart();
        _dispatch_callback(ON_CASSETTE_EJECT);
        if (_gear_is_in_func()) {
            send_command(STOP_COMMAND);
        }
    }
    _prev_has_cassette = _has_cassette;
}

void crp42602y_ctrl_with_counter::_process_command()
{
    // Process command
    if (queue_get_level(&_command_queue) > 0) {
        command_t command;
        queue_peek_blocking(&_command_queue, &command);
        switch (command.type) {
        case CMD_TYPE_STOP:
            if (_stop(command.dir)) {
                _playing = false;
                _cueing = false;
                _playing_for_wait_cue = false;
                _dispatch_callback(ON_STOP);
            }
            queue_remove_blocking(&_command_queue, &command);
            break;
        case CMD_TYPE_PLAY:
            if (_play(command.dir)) {
                _playing = true;
                _cueing = false;
                _playing_for_wait_cue = false;
                if (command.dir == DIR_REVERSE) {
                    _counter.reset();
                    _dispatch_callback(ON_REVERSE);
                } else {
                    _dispatch_callback(ON_PLAY);
                }
            }
            queue_remove_blocking(&_command_queue, &command);
            break;
        case CMD_TYPE_CUE: {
            if (!_is_que_ready_for_counter(command.dir)) {
                bool head_dir_is_a = _head_dir_is_a;
                _cue_dir_is_a = _get_dir_is_a(command.dir);
                if (_play(command.dir)) {
                    _playing = false;
                    _cueing = true;
                    _playing_for_wait_cue = true;
                    // remove CUE command
                    queue_remove_blocking(&_command_queue, &command);
                    // 1. add WAIT command
                    const command_t* wait_command = (command.dir == DIR_FORWARD) ? &WAIT_FF_READY_COMMAND : &WAIT_REW_READY_COMMAND;
                    if (!queue_try_add(&_command_queue, wait_command)) {
                        _dispatch_callback(ON_COMMAND_FIFO_OVERFLOW);
                    }
                    // 2. add HEAD_DIR command
                    const command_t* head_dir_command = (head_dir_is_a) ? &HEAD_DIR_A_COMMAND : &HEAD_DIR_B_COMMAND;
                    if (!queue_try_add(&_command_queue, head_dir_command)) {
                        _dispatch_callback(ON_COMMAND_FIFO_OVERFLOW);
                    }
                    // 3. add original CUE command
                    if (!queue_try_add(&_command_queue, &command)) {
                        _dispatch_callback(ON_COMMAND_FIFO_OVERFLOW);
                    }
                }
            } else {
                if (_cue(command.dir)) {
                    _playing = false;
                    _cueing = true;
                    _playing_for_wait_cue = false;
                    _dispatch_callback(ON_CUE);
                }
                queue_remove_blocking(&_command_queue, &command);
            }
            break;
        }
        case CMD_TYPE_WAIT:
            if (_is_que_ready_for_counter(command.dir)) {
                queue_remove_blocking(&_command_queue, &command);
            }
            break;
        case CMD_TYPE_HEAD_DIR:
            if (command.dir == DIR_FORWARD) {
                _head_dir_is_a = true;
            } else if (command.dir == DIR_BACKWARD) {
                _head_dir_is_a = false;
            }
            queue_remove_blocking(&_command_queue, &command);
            break;
        default:
            break;
        }
        for (int i = NUM_COMMAND_HISTORY_ISSUED - 1; i >= 1; i--) {
            _command_history_issued[i] = _command_history_issued[i - 1];
        }
        _command_history_issued[0] = command;
    }
}

void crp42602y_ctrl_with_counter::_process_callbacks()
{
    // Process callback
    while (queue_get_level(&_callback_queue) > 0) {
        callback_type_t callback_type;
        queue_remove_blocking(&_callback_queue, &callback_type);
        if (callback_type >= __NUM_CALLBACK_TYPE__) {
            if (_callbacks[callback_type - __NUM_CALLBACK_TYPE__] != nullptr) {
                _callbacks[callback_type - __NUM_CALLBACK_TYPE__](callback_type);
            }
        } else {
            if (crp42602y_ctrl::_callbacks[callback_type] != nullptr) {
                crp42602y_ctrl::_callbacks[callback_type](callback_type);
            }
        }
    }
}