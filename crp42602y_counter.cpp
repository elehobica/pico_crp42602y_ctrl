/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

//#include <cstdio>
#include <cmath>

#include "crp42602y_counter.h"

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"

#include "crp42602y_measure_pulse.pio.h"
#include "crp42602y_ctrl.h"

#define CRP42602Y_PIO __CONCAT(pio, PICO_CRP42602Y_CTRL_PIO)
#define PIO_IRQ_x __CONCAT(__CONCAT(PIO, PICO_CRP42602Y_CTRL_PIO), _IRQ_0)  // e.g. PIO0_IRQ_0

crp42602y_counter* crp42602y_counter::_inst_map[4] = {nullptr, nullptr, nullptr, nullptr};

// irq handler for PIO
void __isr __time_critical_func(crp42602y_counter_pio_irq_handler)()
{
    // PIOx_IRQ_0 throws 0 ~ 3 flags corresponding to state machine 0 ~ 3 thanks to 'irq 0 rel' instruction
    for (uint sm = 0; sm < 4; sm++) {
        if (pio_interrupt_get(CRP42602Y_PIO, sm)) {
            pio_interrupt_clear(CRP42602Y_PIO, sm);  // state machine stalls until clear only when timeout
            crp42602y_counter* inst = crp42602y_counter::_inst_map[sm];
            if (inst != nullptr) {
                inst->_irq_callback();  // invoke callback of corresponding instance
            }
        }
    }
}

crp42602y_counter::crp42602y_counter(const uint pin_rotation_sens, crp42602y_ctrl* const ctrl) :
    _ctrl(ctrl), _sm(0),
    _enable(false),
    _status(NONE_BITS), _rot_count(0), _count(0),
    _total_playing_sec{NAN, NAN},
    _estimated_playing_sec{NAN, NAN},
    _last_hub_radius_cm{NAN, NAN},
    _estimated_hub_radius_cm{0.0, 0.0},
    _hub_radius_cm_history{},
    _ref_hub_radius_cm(0.0),
    _tape_thickness_um(DEFAULT_ESTIMATED_TAPE_THICKNESS_UM)
{
    queue_init(&_rotation_event_queue, sizeof(rotation_event_t), ROTATION_EVENT_QUEUE_LENGTH);

    // PIO
    while (pio_sm_is_claimed(CRP42602Y_PIO, _sm)) {
        if (++_sm >= 4) panic("All PIO state machines are reserved");
    }
    pio_sm_claim(CRP42602Y_PIO, _sm);
    _inst_map[_sm] = this;  // link this instance to corresponding state machine here to pass global interrupt to the instance

    // PIO_IRQ
    if (!irq_has_shared_handler(PIO_IRQ_x)) {
        irq_add_shared_handler(PIO_IRQ_x, crp42602y_counter_pio_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    }

    pio_set_irq0_source_enabled(CRP42602Y_PIO, (enum pio_interrupt_source) ((uint) pis_interrupt0 + _sm), true);   // for IRQ relative
    pio_set_irq1_source_enabled(CRP42602Y_PIO, (enum pio_interrupt_source) ((uint) pis_interrupt0 + _sm), false);  // not use
    pio_interrupt_clear(CRP42602Y_PIO, _sm);
    irq_set_enabled(PIO_IRQ_x, true);

    // PIO
    uint offset = pio_add_program(CRP42602Y_PIO, &crp42602y_measure_pulse_program);
    crp42602y_measure_pulse_program_init(
        CRP42602Y_PIO,
        _sm,
        offset,
        crp42602y_measure_pulse_offset_entry_point,
        crp42602y_measure_pulse_program_get_default_config,
        pin_rotation_sens
    );
    pio_sm_put_blocking(CRP42602Y_PIO, _sm, (uint32_t) -TIMEOUT_COUNT);
}

crp42602y_counter::~crp42602y_counter()
{
    queue_free(&_rotation_event_queue);
    _inst_map[_sm] = nullptr;
    pio_sm_unclaim(CRP42602Y_PIO, _sm);

    // Remove handler if there are no instances
    bool exist_inst = false;
    for (int i = 0; i < 4; i++) {
        if (_inst_map[_sm] != nullptr) {
            exist_inst = true;
            break;
        }
    }
    if (!exist_inst) {
        if (irq_has_shared_handler(PIO_IRQ_x)) {
            irq_remove_handler(PIO_IRQ_x, crp42602y_counter_pio_irq_handler);
        }
    }
}

void crp42602y_counter::enable_counter()
{
    _enable = true;
}

void crp42602y_counter::restart()
{
    _status = NONE_BITS;
    _rot_count = 0;
    _count = 0;
    for (int i = 0; i < 2; i++) {
        _total_playing_sec[i] = NAN;
        _estimated_playing_sec[i] = NAN;
        _last_hub_radius_cm[i] = NAN;
        _estimated_hub_radius_cm[i] = 0.0;
    }
    for (int i = 0; i < MAX_NUM_TO_AVERAGE; i++) {
        _hub_radius_cm_history[i] = 0;
    }
    _ref_hub_radius_cm = 0.0;
    _tape_thickness_um = DEFAULT_ESTIMATED_TAPE_THICKNESS_UM;
}

float crp42602y_counter::get() const
{
    bool is_dir_a = _ctrl->get_head_dir_is_a();
    if (_check_status(TIME_BIT)) {
        return _total_playing_sec[!is_dir_a];
    } else {
        return NAN;
    }
}

void crp42602y_counter::reset()
{
    bool is_dir_a = _ctrl->get_head_dir_is_a();
    if (_check_status(TIME_BIT)) {
        _total_playing_sec[!is_dir_a] = 0.0;
    }
}

uint32_t crp42602y_counter::get_state() const
{
    if (_check_status(ALL_BITS)) {
        return FULL_READY;
    } else if (_check_status(THICKNESS_BIT) && !_check_status(RADIUS_A_BIT | RADIUS_B_BIT)) {
        return EITHER_CUE_READY;
    } else if (_check_status(TIME_BIT)) {
        return PLAY_ONLY;
    } else {
        return UNDETERMINED;
    }
}

bool crp42602y_counter::_check_status(uint32_t bits) const
{
    return (_status & bits) == bits;
}

float crp42602y_counter::_correct_tape_thickness_um(float tape_thickness_um)
{
    // standardize value
    if (tape_thickness_um < 7.5) {
        tape_thickness_um = 6.0;  // correct to 6 um (C-120)
    } else if (tape_thickness_um < 10.5) {
        tape_thickness_um = 9.0;  // correct to 9 um (C-100)
    } else if (tape_thickness_um < 15.0) {
        tape_thickness_um = 12.0;  // correct to 12 um (C-90)
    } else {
        tape_thickness_um = 18.0;  // correct to 18 um (C-60, C-46)
    }
    return tape_thickness_um;
}

void crp42602y_counter::_irq_callback()
{
    uint32_t accum_time_us = 0;
    while (pio_sm_get_rx_fifo_level(CRP42602Y_PIO, _sm)) {
        uint32_t val = pio_sm_get_blocking(CRP42602Y_PIO, _sm);
        if (val == 0) {  // timeout
            accum_time_us = 0;
            break;
        }
        accum_time_us += -((int32_t) val) * PIO_COUNT_DIV;  // val is always negative value
    }
    bool is_playing = _ctrl->is_playing();
    bool is_cueing = _ctrl->is_cueing();
    bool is_playing_internal = _ctrl->_is_playing_internal();
    bool gear_is_changing = _ctrl->_gear_is_changing();
    bool head_dir_is_a = _ctrl->get_head_dir_is_a();
    bool cue_dir_is_a = _ctrl->get_cue_dir_is_a();
    // Discard dummy rotations
    if ((!is_playing && !is_cueing && !is_playing_internal) || gear_is_changing) {
        pio_sm_put_blocking(CRP42602Y_PIO, _sm, (uint32_t) -TIMEOUT_COUNT);
        _rot_count = 0;
        return;
    }
    // Timeout, thus rotation stopped
    if (accum_time_us == 0) {
        pio_sm_put_blocking(CRP42602Y_PIO, _sm, (uint32_t) -TIMEOUT_COUNT);
        _rot_count = 0;
        _ctrl->_on_rotation_stop();
        return;
    }
    //printf("%d\r\n", accum_time_us);
    // After dummy rotations:
    //   1st time: wrong interval
    //   2nd time: sometimes inaccurate interval
    if (_rot_count > 1) {  // wait 2 times for early stop detection because of lack of accuracy
        pio_sm_put_blocking(CRP42602Y_PIO, _sm, (uint32_t) ((int32_t) -(accum_time_us / PIO_COUNT_DIV)));
    } else {
        pio_sm_put_blocking(CRP42602Y_PIO, _sm, (uint32_t) -TIMEOUT_COUNT);
    }
    if (!_enable) {
        _rot_count++;
        return;
    }

    if (_rot_count > 0) {  // ignore 1 time to avoid wrong interval inforamtion
        if (is_playing || is_playing_internal) {
            rotation_event_t event = {
                accum_time_us + ADDITIONAL_US,
                PLAY,
                head_dir_is_a,
                _rot_count - 1
            };
            if (!queue_try_add(&_rotation_event_queue, &event)) {
                _ctrl->_dispatch_callback((crp42602y_ctrl::callback_type_t) crp42602y_ctrl_with_counter::ON_COUNTER_FIFO_OVERFLOW);
            }
        } else if (is_cueing) {
            rotation_event_t event = {
                accum_time_us + ADDITIONAL_US,
                CUE,
                cue_dir_is_a,
                _rot_count - 1
            };
            if (!queue_try_add(&_rotation_event_queue, &event)) {
                _ctrl->_dispatch_callback((crp42602y_ctrl::callback_type_t) crp42602y_ctrl_with_counter::ON_COUNTER_FIFO_OVERFLOW);
            }
        }
    }
    if (_rot_count < MAX_NUM_TO_AVERAGE + 1) _rot_count++;
}

void crp42602y_counter::_process()
{
    while (queue_get_level(&_rotation_event_queue) > 0) {
        rotation_event_t event;
        queue_remove_blocking(&_rotation_event_queue, &event);
        // reset count if function (play/cue) has changed
        if (event.num_to_average == 0) _count = 0;

        if (event.type == PLAY) {
            _process_play(event);
        } else if (event.type == CUE) {
            _process_cue(event);
        }
    }
}

void crp42602y_counter::_process_play(const rotation_event_t& event)
{
    int fs = (int) !event.is_dir_a; // front side
    int bs = 1 - fs; // back side

    // rotation calculation
    float rotation_per_second = 1.0e6 / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO / event.interval_us;
    float hub_radius_cm = TAPE_SPEED_CM_PER_SEC / 2.0 / M_PI / rotation_per_second;
    float hub_rotations = (float) _count / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO;
    float tape_length = TAPE_SPEED_CM_PER_SEC * event.interval_us / 1e6;
    // reflect to total playing sec
    float add_time = event.interval_us / 1e6;
    if (_status == NONE_BITS) {
        _total_playing_sec[fs] = 0.0;
        _total_playing_sec[bs] = 0.0;
        _estimated_playing_sec[fs] = 0.0;
        _estimated_playing_sec[bs] = 0.0;
        _status |= TIME_BIT;
    }
    _total_playing_sec[fs] += add_time;
    _total_playing_sec[bs] -= add_time;

    // from here, exclude 1st interval due to potential inaccuracy
    if (event.num_to_average == 0) return;

    // shift hub_radius history
    for (int i = MAX_NUM_TO_AVERAGE - 1; i > 0; i--) {
        _hub_radius_cm_history[i] = _hub_radius_cm_history[i - 1];
    }
    _hub_radius_cm_history[0] = hub_radius_cm;
    // average of hub_radius
    float average_hub_radius_cm = 0.0;
    for (int i = 0; i < event.num_to_average; i++) {
        average_hub_radius_cm += _hub_radius_cm_history[i];
    }
    average_hub_radius_cm /= event.num_to_average;

    // [1] Tape thickness measurement at transition from CUE to PLAY
    if (event.num_to_average == 1 && !_check_status(THICKNESS_BIT) && _estimated_hub_radius_cm[fs] > DEFAULT_ESTIMATED_TAPE_THICKNESS_UM / 1e4 * 10) {
        float estimated_rotations = _estimated_hub_radius_cm[fs] / (DEFAULT_ESTIMATED_TAPE_THICKNESS_UM / 1e4);
        float original_hub_radius_cm = _last_hub_radius_cm[fs] - _estimated_hub_radius_cm[fs];
        float actual_diff_hub_radius_cm = average_hub_radius_cm - original_hub_radius_cm;
        // calculate tape thickness (this value should not be corrected because (radius diff / rotations) is more accurate)
        float tape_thickness_um = actual_diff_hub_radius_cm / estimated_rotations * 1e4;
        // calculate diff area, then it leads diff tape length -> diff sec
        float actual_diff_area = M_PI * (pow(average_hub_radius_cm, 2.0) - pow(original_hub_radius_cm, 2.0));
        float actual_diff_sec = actual_diff_area / (TAPE_SPEED_CM_PER_SEC * tape_thickness_um / 1e4);
        /*
        printf("----------------------------\r\n");
        printf("original hub radius: %7.4f\r\n", _last_hub_radius_cm[fs] - _estimated_hub_radius_cm[fs]);
        printf("current hub radius: %7.4f\r\n", _last_hub_radius_cm[fs]);
        printf("estimated hub radius: %7.4f\r\n", _estimated_hub_radius_cm[fs]);
        printf("actual hub radius: %7.4f\r\n", average_hub_radius_cm);
        printf("threshold hub radius: %7.4f\r\n", DEFAULT_ESTIMATED_TAPE_THICKNESS_UM / 1e4 * 10);
        printf("calculated tape_thickness_um: %7.4f\r\n", tape_thickness_um);
        printf("estimated diff sec = %7.4f\r\n", _estimated_playing_sec[fs]);
        printf("actual diff sec = %7.4f\r\n", actual_diff_sec);
        printf("----------------------------\r\n");
        */
        float error_sec = _estimated_playing_sec[fs] - actual_diff_sec;
        _total_playing_sec[fs] -= error_sec;
        _total_playing_sec[bs] += error_sec;
        if (_check_status(RADIUS_A_BIT << bs)) {
            original_hub_radius_cm = _last_hub_radius_cm[bs] - _estimated_hub_radius_cm[bs];
            _last_hub_radius_cm[bs] = sqrt(pow(original_hub_radius_cm, 2.0) - actual_diff_area / M_PI);
            _estimated_hub_radius_cm[bs] = 0.0;
        }
        _estimated_playing_sec[fs] = 0.0;
        _estimated_playing_sec[bs] = 0.0;
        _estimated_hub_radius_cm[fs] = 0.0;
        _tape_thickness_um = _correct_tape_thickness_um(tape_thickness_um);
        _status |= THICKNESS_BIT;
    }
    _last_hub_radius_cm[fs] = average_hub_radius_cm;
    _status |= RADIUS_A_BIT << fs;

    // [2] Tape thickness measurement during PLAY
    if (_count == 40) {  // this is reference (need to avoid tape leader)
        _ref_hub_radius_cm = average_hub_radius_cm;
    } else if ((!_check_status(THICKNESS_BIT) && _count >= 100 && _count % 10 == 0) ||
                (_count >= 200 && _count % 100 == 0)) {  // calculate diff from reference
        float diff_hub_radius_cm = average_hub_radius_cm - _ref_hub_radius_cm;
        float diff_hub_rotations = 1.0 / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO * (_count - 40);
        _tape_thickness_um = diff_hub_radius_cm * 1e4 / diff_hub_rotations;
        _tape_thickness_um = _correct_tape_thickness_um(_tape_thickness_um);
        if (!_check_status(THICKNESS_BIT)) {
            float compensation_ratio= 1.0 - _tape_thickness_um / DEFAULT_ESTIMATED_TAPE_THICKNESS_UM;
            _total_playing_sec[fs] -= _estimated_playing_sec[fs] * compensation_ratio;
            _total_playing_sec[bs] -= _estimated_playing_sec[bs] * compensation_ratio;
            _last_hub_radius_cm[fs] -= _estimated_hub_radius_cm[fs] * compensation_ratio;
            _last_hub_radius_cm[bs] -= _estimated_hub_radius_cm[bs] * compensation_ratio;
            _estimated_playing_sec[fs] = 0.0;
            _estimated_playing_sec[bs] = 0.0;
            _estimated_hub_radius_cm[fs] = 0.0;
            _estimated_hub_radius_cm[bs] = 0.0;
            _status |= THICKNESS_BIT;
        }
    }
    // reflect to back side of _last_hub_radius_cm
    if (_check_status(RADIUS_A_BIT << bs)) {
        _last_hub_radius_cm[bs] -= _tape_thickness_um / 1e4 * tape_length / (2.0 * M_PI * _last_hub_radius_cm[bs]);
        if (!_check_status(THICKNESS_BIT)) {
            _estimated_hub_radius_cm[bs] -= _tape_thickness_um / 1e4 * tape_length / (2.0 * M_PI * _last_hub_radius_cm[bs]);
        }
    }
    /*
    if (_count % 10 == 0) {
        printf("--------\r\n");
        printf("interval_us = %d\r\n", (int) event.interval_us);
        printf("rps = %7.4f\r\n", rotation_per_second);
        printf("hub radius[fs] = %7.4f\r\n", _last_hub_radius_cm[fs]);
        printf("hub radius[bs] = %7.4f\r\n", _last_hub_radius_cm[bs]);
        printf("hub rotations = %7.4f\r\n", hub_rotations);
        printf("tape thicknesss (um)= %7.4f\r\n", _tape_thickness_um);
        printf("time A = %7.4f\r\n", _total_playing_sec[0]);
        printf("time B = %7.4f\r\n", _total_playing_sec[1]);
        printf("_status = %04b\r\n", _status);
        printf("_count = %d\r\n", _count);
    }
    */
    _count++;
}

void crp42602y_counter::_process_cue(const rotation_event_t& event)
{
    int fs = (int) !event.is_dir_a; // front side
    int bs = 1 - fs; // back side

    // rotation calculation
    float hub_rotations = (float) _count / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO;
    float diff_hub_rotations = 1.0 / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO;
    if (!_check_status(RADIUS_A_BIT << fs)) {
        _total_playing_sec[fs] = NAN;
        _total_playing_sec[bs] = NAN;
        _estimated_playing_sec[fs] = NAN;
        _estimated_playing_sec[bs] = NAN;
        _count = 0;
        _status = NONE_BITS;
    } else if (_check_status(TIME_BIT)) {
        float tape_length = 2.0 * M_PI * _last_hub_radius_cm[fs] * diff_hub_rotations;
        float add_time = tape_length / TAPE_SPEED_CM_PER_SEC;
        //printf("%7.4f %7.4f %7.4f %7.4f\r\n", add_time, hub_rotations, _last_hub_radius_cm[fs], diff_hub_rotations);
        _total_playing_sec[fs] += add_time;
        _total_playing_sec[bs] -= add_time;
        _last_hub_radius_cm[fs] += _tape_thickness_um / 1e4 * diff_hub_rotations;
        if (_check_status(RADIUS_A_BIT << bs)) {
            _last_hub_radius_cm[bs] -= _tape_thickness_um / 1e4 * tape_length / (2.0 * M_PI * _last_hub_radius_cm[bs]);
        }
        if (!_check_status(THICKNESS_BIT)) {
            _estimated_playing_sec[fs] += add_time;
            _estimated_playing_sec[bs] -= add_time;
            _estimated_hub_radius_cm[fs] += _tape_thickness_um / 1e4 * diff_hub_rotations;
            if (_check_status(RADIUS_A_BIT << bs)) {
                _estimated_hub_radius_cm[bs] -= _tape_thickness_um / 1e4 * tape_length / (2.0 * M_PI * _last_hub_radius_cm[bs]);
            }
        }
        /*
        if (_count % 10 == 0) {
            printf("--------\r\n");
            printf("hub radius[fs] = %7.4f\r\n", _last_hub_radius_cm[fs]);
            printf("hub radius[bs] = %7.4f\r\n", _last_hub_radius_cm[bs]);
            printf("tape thicknesss (um)= %7.4f\r\n", _tape_thickness_um);
            printf("time A = %7.4f\r\n", _total_playing_sec[0]);
            printf("time B = %7.4f\r\n", _total_playing_sec[1]);
            printf("_status = %04b\r\n", _status);
            printf("_count = %d\r\n", _count);
        }
        */
        _count++;
    }
}