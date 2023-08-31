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

#include <map>

class rotation_calc {
    public:
    rotation_calc(uint pin_rotation_sens);
    virtual ~rotation_calc();
    void irq_callback();
    void mark_half_rotation();

    private:
    static constexpr float    TAPE_SPEED_CM_PER_SEC = 4.75;
    static constexpr int      NUM_ROTATION_WINGS = 2;
    static constexpr float    ROTATION_GEAR_RATIO = 43.0 / 23.0;
    static constexpr uint32_t TIMEOUT_COUNT = (uint32_t) (1000000 / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO);

    static std::map<uint, rotation_calc*> _inst_map;

    int _count;
    uint _sm;
    uint64_t _prev_time;
    friend void crp42602y_ctrl_pio_irq_handler();
};