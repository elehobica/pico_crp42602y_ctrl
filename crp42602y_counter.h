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

class crp42602y_ctrl;  // reference to avoid inter lock

class crp42602y_counter {
    public:
    crp42602y_counter(uint pin_rotation_sens, crp42602y_ctrl* ctrl);
    virtual ~crp42602y_counter();
    void irq_callback();

    private:
    static constexpr float    TAPE_SPEED_CM_PER_SEC = 4.75;
    static constexpr uint32_t NUM_ROTATION_WINGS = 2;
    static constexpr float    ROTATION_GEAR_RATIO = 43.0 / 23.0;
    static constexpr uint32_t TIMEOUT_MILLI_SEC = 1000;
    static constexpr uint32_t PIO_FREQUENCY_HZ = 1000000;
    static constexpr uint32_t PIO_COUNT_DIV = 4;
    static constexpr uint32_t TIMEOUT_COUNT = TIMEOUT_MILLI_SEC * PIO_FREQUENCY_HZ / 1000 / PIO_COUNT_DIV;
    static constexpr uint32_t ADDITIONAL_US = 5 + 4;  // additional cycles from PIO program

    static crp42602y_counter* _inst_map[4];

    crp42602y_ctrl* _ctrl;
    int _count;
    uint _sm;
    uint32_t _accum_time_us_history[4];

    void _mark_half_rotation(uint32_t interval_us);

    friend void crp42602y_ctrl_pio_irq_handler();
};