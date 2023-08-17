/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "crp42602y_ctrl.h"

//#include <cstdio>
#include "hardware/pwm.h"

crp42602y_ctrl::crp42602y_ctrl(
    uint pin_cassette_detect,
    uint pin_gear_status_sw,
    uint pin_rotation_sens,
    uint pin_solenoid_ctrl,
    uint pin_rec_a_sw,
    uint pin_rec_b_sw,
    uint pin_type2_sw
    ) :
    _pin_cassette_detect(pin_cassette_detect),
    _pin_gear_status_sw(pin_gear_status_sw),
    _pin_rotation_sens(pin_rotation_sens),
    _pin_solenoid_ctrl(pin_solenoid_ctrl),
    _pin_rec_a_sw(pin_rec_a_sw),
    _pin_rec_b_sw(pin_rec_b_sw),
    _pin_type2_sw(pin_type2_sw),
    _head_dir_a(true),
    _has_cassette(false),
    _reverse_mode(RVS_ONE_ROUND),
    _playing(false),
    _cueing(false)
{
    queue_init(&_command_queue, sizeof(command_t), COMMAND_QUEUE_LENGTH);

    // PWM for _pin_rotation_sens
    gpio_set_function(_pin_rotation_sens, GPIO_FUNC_PWM);
    _pwm_slice_num = pwm_gpio_to_slice_num(_pin_rotation_sens);
    pwm_config pwm_config0 = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&pwm_config0, PWM_DIV_B_RISING);
    pwm_config_set_clkdiv_int(&pwm_config0, 1);
    pwm_init(_pwm_slice_num, &pwm_config0, true);

    for (int i = 0; i < sizeof(_rot_count_history) / sizeof(uint16_t); i++) {
        _rot_count_history[i] = 0;
    }
}

void crp42602y_ctrl::periodic_func_100ms()
{

    _has_cassette = !gpio_get(_pin_cassette_detect);

    uint16_t rot_count = pwm_get_counter(_pwm_slice_num);
    uint16_t rot_compare = _rot_count_history[sizeof(_rot_count_history) / sizeof(uint16_t) - 1];
    uint16_t rot_diff;
    if (rot_count < rot_compare) {
        rot_diff = (uint16_t) ((1U << 16) + rot_count - rot_compare);
    } else {
        rot_diff = rot_count - rot_compare;
    }
    bool rotating = rot_diff > 0;
    //printf("pwm counter = %d, rotating = %d\r\n", (int) rot_count, rotating ? 1 : 0);
    for (int i = sizeof(_rot_count_history) / sizeof(uint16_t) - 1; i >= 1; i--) {
        _rot_count_history[i] = _rot_count_history[i - 1];
    }
    _rot_count_history[0] = rot_count;

    // Stop action by reverse mode
    if (_is_gear_in_func() && !rotating) {
        command_t command;
        switch (_reverse_mode) {
        case RVS_ONE_WAY:
            command.type = CMD_TYPE_STOP;
            command.dir = DIR_KEEP;
            break;
        case RVS_ONE_ROUND: {
            if (_head_dir_a) {
                command.type = CMD_TYPE_PLAY;
                command.dir = DIR_REVERSE;
            } else {
                command.type = CMD_TYPE_STOP;
                command.dir = DIR_KEEP;
            }
            break;
        }
        case RVS_INFINITE_ROUND:
            command.type = CMD_TYPE_PLAY;
            command.dir = DIR_REVERSE;
            break;
        default:
            command.type = CMD_TYPE_STOP;
            command.dir = DIR_KEEP;
            break;
        }
        send_command(command);
    }
}

bool crp42602y_ctrl::is_playing()
{
    return _playing;
}

bool crp42602y_ctrl::is_cueing()
{
    return _cueing;
}

bool crp42602y_ctrl::is_dir_a()
{
    return _head_dir_a;
}

bool crp42602y_ctrl::send_command(const command_t& command)
{
    if (queue_get_level(&_command_queue) < COMMAND_QUEUE_LENGTH) {
        return queue_try_add(&_command_queue, &command);
    }
    return false;
}

void crp42602y_ctrl::process_loop()
{
    uint count = queue_get_level(&_command_queue);
    if (count) {
        command_t command;
        queue_remove_blocking(&_command_queue, &command);
        switch (command.type) {
        case CMD_TYPE_STOP:
            _stop();
            _playing = false;
            _cueing = false;
            break;
        case CMD_TYPE_PLAY:
            _play(command.dir);
            _playing = true;
            _cueing = false;
            break;
        case CMD_TYPE_CUE:
            _cue(command.dir);
            _playing = false;
            _cueing = true;
            break;
        default:
            break;
        }
    }
}

void crp42602y_ctrl::_pull_solenoid(bool flag)
{
    gpio_put(_pin_solenoid_ctrl, !flag);
}

bool crp42602y_ctrl::_is_gear_in_func()
{
    return !gpio_get(_pin_gear_status_sw);
}

void crp42602y_ctrl::_func_sequence(bool head_dir_a, bool lift_head, bool reel_fwd)
{
    // Function sequence has 190 degree of function gear to roll in 400 ms
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
    _pull_solenoid(!head_dir_a);
    sleep_ms(tHeadDirE - tInitE);
    _pull_solenoid(false);
    sleep_ms(tLiftHeadS - tHeadDirE);
    _pull_solenoid(lift_head);
    sleep_ms(tLiftHeadE - tLiftHeadS);
    _pull_solenoid(reel_fwd);
    sleep_ms(tReelE - tLiftHeadE);
    _pull_solenoid(false);
}

void crp42602y_ctrl::_return_sequence()
{
    // Return sequence has (360 - 190) degree of function gear,
    //  which is needed to take another function when the gear is already in function position
    //  it is supposed to take 360 ms
    _pull_solenoid(true);
    sleep_ms(20);
    _pull_solenoid(false);
    sleep_ms(340);
    sleep_ms(20);  // additional margin
}

void crp42602y_ctrl::_stop()
{
    if (_is_gear_in_func()) {
        _return_sequence();
    }
}

bool crp42602y_ctrl::_get_abs_dir(direction_t dir)
{
    bool dir_abs;
    switch(dir) {
    case DIR_KEEP:
        dir_abs = _head_dir_a;
        break;
    case DIR_REVERSE:
        dir_abs = !_head_dir_a;
        break;
    case DIR_FWD:
        dir_abs = true;
        break;
    case DIR_RWD:
        dir_abs = false;
        break;
    default:
        dir_abs = _head_dir_a;
        break;
    }
    return dir_abs;
}

void crp42602y_ctrl::_play(direction_t dir)
{
    _head_dir_a = _get_abs_dir(dir);
    if (_is_gear_in_func()) _return_sequence();
    if (!_has_cassette) return;
    _func_sequence(_head_dir_a, true, _head_dir_a);
}

void crp42602y_ctrl::_cue(direction_t dir)
{
    bool cue_dir_fwd = _get_abs_dir(dir);
    if (_is_gear_in_func()) _return_sequence();
    if (!_has_cassette) return;
    // Evacuate head, however the head direction still matters for which side the head is tracing,
    _func_sequence(_head_dir_a, false, cue_dir_fwd);
}
