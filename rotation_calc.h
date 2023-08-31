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

class crp42602y_ctrl;  // reference to avoid inter lock

class rotation_calc {
    public:
    rotation_calc(uint pin_rotation_sens, crp42602y_ctrl* ctrl);
    virtual ~rotation_calc();
    void irq_callback();

    private:
    static constexpr float    TAPE_SPEED_CM_PER_SEC = 4.75;
    static constexpr uint32_t NUM_ROTATION_WINGS = 2;
    static constexpr float    ROTATION_GEAR_RATIO = 43.0 / 23.0;
    static constexpr uint32_t PIO_MICRO_SEC_PER_COUNT = 4;
    static constexpr uint32_t TIMEOUT_COUNT = (uint32_t) (1000000 / PIO_MICRO_SEC_PER_COUNT);

    static std::map<uint, rotation_calc*> _inst_map;

    crp42602y_ctrl* _ctrl;
    int _count;
    uint _sm;

    void _mark_half_rotation(uint32_t interval_us);
    void _assert_stop_detection();

    friend void crp42602y_ctrl_pio_irq_handler();
};