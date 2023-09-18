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

// references to avoid inter lock
class crp42602y_ctrl;
class crp42602y_ctrl_with_counter;

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
    /**
     * counter state
     */
    typedef enum _counter_state_t {
        UNDETERMINED = 0,
        PLAY_ONLY,
        PLAY_AND_EITHER_CUE_READY,
        FULL_READY
    } counter_state_t;

    /**
     * crp42602y_counter class constructor
     *
     * @param[in] pin_rotation_sens GPIO Input: Rotation sensor
     * @param[in] ctrl              pointer of crp42602y_ctrl instance
     */
    crp42602y_counter(const uint pin_rotation_sens, crp42602y_ctrl* const ctrl);

    /**
     * crp42602y_counter class destructor
     */
    virtual ~crp42602y_counter();

    /**
     * restart the counter
     *   it is supposed that this is called when the cassette has replaced
     */
    void restart();

    /**
     * get the counter value of current side
     */
    float get() const;

    /**
     * reset the counter value of current side
     */
    void reset();

    /**
     * get the counter state
     */
    uint32_t get_state() const;

    private:
    typedef enum _rotation_event_type_t {
        PLAY = 0,
        CUE
    } rotation_event_type_t;

    typedef struct _rotation_event_t {
        uint32_t interval_us;
        rotation_event_type_t type;
        bool is_dir_a;
        int num_to_average;  // 0 ~ MAX_NUM_TO_AVERAGE: 0 means not to use for average
    } rotation_event_t;
    typedef enum _counter_status_bit_t {
        NONE_BITS     = 0,
        TIME_BIT      = (1 << 0),
        RADIUS_A_BIT  = (1 << 1),
        RADIUS_B_BIT  = (1 << 2),
        THICKNESS_BIT = (1 << 3),
        ALL_BITS      = TIME_BIT | RADIUS_A_BIT | RADIUS_B_BIT | THICKNESS_BIT
    } counter_status_bit_t;

    static constexpr uint32_t NUM_ROTATION_WINGS = 2;  // determined by the physical wing number of rotation sensor obstacle
    static constexpr float    ROTATION_GEAR_RATIO = 43.0 / 23.0;  // detemined by the gear teeth number ratio of hub and rotation sensor obstacle
    static constexpr uint32_t TIMEOUT_MILLI_SEC = 1000;
    static constexpr uint32_t PIO_FREQUENCY_HZ = 1000000;
    static constexpr uint32_t PIO_COUNT_DIV = 4;
    static constexpr uint32_t TIMEOUT_COUNT = TIMEOUT_MILLI_SEC * PIO_FREQUENCY_HZ / 1000 / PIO_COUNT_DIV;
    static constexpr uint32_t ADDITIONAL_US = 5 + 4;  // additional cycles from PIO program
    static constexpr uint     ROTATION_EVENT_QUEUE_LENGTH = 4;
    static constexpr float    TAPE_SPEED_CM_PER_SEC = 4.75;
    static constexpr float    DEFAULT_ESTIMATED_TAPE_THICKNESS_UM = 18.0;
    static constexpr uint32_t MAX_NUM_TO_AVERAGE = 20;

    static crp42602y_counter* _inst_map[4];

    crp42602y_ctrl* const _ctrl;
    uint _sm;
    bool _enable;
    uint32_t _status;
    int _rot_count;
    int _count;
    float _total_playing_sec[2];
    float _estimated_playing_sec[2];
    float _last_hub_radius_cm[2];
    float _estimated_hub_radius_cm[2];
    float _hub_radius_cm_history[MAX_NUM_TO_AVERAGE];
    float _ref_hub_radius_cm;
    float _tape_thickness_um;
    queue_t _rotation_event_queue;

    void _enable_counter();
    bool _check_status(uint32_t bits) const;
    float _correct_tape_thickness_um(float tape_thickness_um);
    void _irq_callback();
    void _process();
    void _process_play(const rotation_event_t& event);
    void _process_cue(const rotation_event_t& event);

    friend crp42602y_ctrl;
    friend crp42602y_ctrl_with_counter;
    friend void crp42602y_counter_pio_irq_handler();
};