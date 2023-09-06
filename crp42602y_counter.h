/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

#if !defined(PICO_CRP42602Y_CTRL_PIO)
#define PICO_CRP42602Y_CTRL_PIO 0
#endif

#if !defined(PICO_CRP42602Y_CTRL_PIO_IRQ)
#define PICO_CRP42602Y_CTRL_PIO_IRQ 0
#endif

#include "pico/util/queue.h"

class crp42602y_ctrl;  // reference to avoid inter lock

// Assumption:
// Play:   Rotation sensor is attached to the hub rolling up.
//         Hub rotation speeed depends on the tape speed (4.75 cm/sec)
//         Threrefore, the hub radius and tape thickness can be calculated from the rotation speed,
//         Ascending time is equal to the time passing.
// FF/REW: Rotation sensor is attached to the hub rolling up.
//         Hub rotation speeed is constant.
//         Threrefore, if both the hub radius of rolling up side and tape thickness are known,
//         processing time can be calculated.

class crp42602y_counter {
    public:
    typedef enum _counter_state_t {
        UNDETERMINED = 0,
        PLAY_ONLY,
        EITHER_CUE_READY,
        FULL_READY
    } counter_state_t;

    crp42602y_counter(const uint pin_rotation_sens, crp42602y_ctrl* const ctrl);
    virtual ~crp42602y_counter();
    float get_counter();
    void reset_counter();
    uint32_t get_counter_state();

    private:
    typedef enum _counter_status_bit_t {
        NONE_BITS     = 0,
        TIME_BIT      = (1 << 0),
        RADIUS_A_BIT  = (1 << 1),
        RADIUS_B_BIT  = (1 << 2),
        THICKNESS_BIT = (1 << 3),
        ALL_BITS      = TIME_BIT | RADIUS_A_BIT | RADIUS_B_BIT | THICKNESS_BIT
    } counter_status_bit_t;

    static constexpr float    TAPE_SPEED_CM_PER_SEC = 4.75;
    static constexpr uint32_t NUM_ROTATION_WINGS = 2;
    static constexpr float    ROTATION_GEAR_RATIO = 43.0 / 23.0;
    static constexpr uint32_t TIMEOUT_MILLI_SEC = 1000;
    static constexpr uint32_t PIO_FREQUENCY_HZ = 1000000;
    static constexpr uint32_t PIO_COUNT_DIV = 4;
    static constexpr uint32_t TIMEOUT_COUNT = TIMEOUT_MILLI_SEC * PIO_FREQUENCY_HZ / 1000 / PIO_COUNT_DIV;
    static constexpr uint32_t ADDITIONAL_US = 5 + 4;  // additional cycles from PIO program
    static constexpr uint     ROTATION_EVENT_QUEUE_LENGTH = 4;
    static constexpr int      NUM_HUB_ROTATION_HISTORY = 15;
    static constexpr int      NUM_IGNORE_HUB_ROTATION_HISTORY1 = 5;
    static constexpr int      NUM_IGNORE_HUB_ROTATION_HISTORY2 = 2;

    static crp42602y_counter* _inst_map[4];

    crp42602y_ctrl* const _ctrl;
    uint32_t _status;
    int _count;
    uint _sm;
    uint32_t _accum_time_us_history[4];
    float _total_playing_sec[2];
    float _last_hub_radius_cm[2];
    float _hub_radius_cm_history[NUM_HUB_ROTATION_HISTORY - NUM_IGNORE_HUB_ROTATION_HISTORY2];
    float _ref_hub_radius_cm;
    int   _num_average;
    float _average_hub_radius_cm;
    float _tape_thickness_um;
    queue_t _rotation_event_queue;

    bool _check_status(uint32_t bits);
    void _correct_tape_thickness_um();
    void _irq_callback();
    void _process();

    friend crp42602y_ctrl;
    friend void crp42602y_counter_pio_irq_handler();
};