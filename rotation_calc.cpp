/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <cstdio>
#include <cmath>

#include "rotation_calc.h"

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"

#include "rotation_term.pio.h"
#include "crp42602y_ctrl.h"

#define CRP42602Y_PIO __CONCAT(pio, PICO_CRP42602Y_CTRL_PIO)
#define PIO_IRQ_x __CONCAT(__CONCAT(PIO, PICO_CRP42602Y_CTRL_PIO), _IRQ_0)  // e.g. PIO0_IRQ_0

rotation_calc* rotation_calc::_inst_map[4] = {nullptr, nullptr, nullptr, nullptr};

// irq handler for PIO
void __isr __time_critical_func(crp42602y_ctrl_pio_irq_handler)()
{
    // PIOx_IRQ_0 throws 0 ~ 3 flags corresponding to state machine 0 ~ 3 thanks to 'irq 0 rel' instruction
    for (uint sm = 0; sm < 4; sm++) {
        if (pio_interrupt_get(CRP42602Y_PIO, sm)) {
            pio_interrupt_clear(CRP42602Y_PIO, sm);  // state machine stalls until clear only when timeout
            rotation_calc* rc = rotation_calc::_inst_map[sm];
            if (rc != nullptr) {
                rc->irq_callback();  // invoke callback of corresponding instance
            }
        }
    }
}

rotation_calc::rotation_calc(uint pin_rotation_sens, crp42602y_ctrl* ctrl) :
    _ctrl(ctrl), _count(0), _sm(0), _accum_time_us_history{}
{
    // PIO
    while (pio_sm_is_claimed(CRP42602Y_PIO, _sm)) {
        if (++_sm >= 4) panic("All PIO state machines are reserved");
    }
    pio_sm_claim(CRP42602Y_PIO, _sm);
    _inst_map[_sm] = this;  // link this instance to corresponding state machine here to pass global interrupt to the instance

    // PIO_IRQ
    if (!irq_has_shared_handler(PIO_IRQ_x)) {
        irq_add_shared_handler(PIO_IRQ_x, crp42602y_ctrl_pio_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    }

    pio_set_irq0_source_enabled(CRP42602Y_PIO, (enum pio_interrupt_source) ((uint) pis_interrupt0 + _sm), true);   // for IRQ relative
    pio_set_irq1_source_enabled(CRP42602Y_PIO, (enum pio_interrupt_source) ((uint) pis_interrupt0 + _sm), false);  // not use
    pio_interrupt_clear(CRP42602Y_PIO, _sm);
    irq_set_enabled(PIO_IRQ_x, true);

    // PIO
    uint offset = pio_add_program(CRP42602Y_PIO, &rotation_term_program);
    rotation_term_program_init(
        CRP42602Y_PIO,
        _sm,
        offset,
        rotation_term_offset_entry_point,
        rotation_term_program_get_default_config,
        pin_rotation_sens
    );
    pio_sm_put_blocking(CRP42602Y_PIO, _sm, (uint32_t) (uint32_t) -TIMEOUT_COUNT);
}

rotation_calc::~rotation_calc()
{
    _inst_map[_sm] = nullptr;
    pio_sm_unclaim(CRP42602Y_PIO, _sm);
    /*
    // need confirm if other instance still uses the handler
    if (irq_has_shared_handler(PIO_IRQ_x)) {
        irq_remove_handler(PIO_PIO_x, crp42602y_ctrl_pio_irq_handler);
    }
    */
}

void rotation_calc::irq_callback()
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
    if (accum_time_us == 0) {
        pio_sm_put_blocking(CRP42602Y_PIO, _sm, (uint32_t) (uint32_t) -TIMEOUT_COUNT);
        _ctrl->on_rotation_stop();
    } else {
        bool stable = true;
        for (int i = 0; i < sizeof(_accum_time_us_history) / sizeof(uint32_t); i++) {
            if (accum_time_us < _accum_time_us_history[i] * 7/8 || accum_time_us > _accum_time_us_history[i] * 9/8) {
                stable = false;
                break;
            }
        }
        if (stable) {
            pio_sm_put_blocking(CRP42602Y_PIO, _sm, (uint32_t) ((int32_t) -(accum_time_us / PIO_COUNT_DIV)));
        } else {
            pio_sm_put_blocking(CRP42602Y_PIO, _sm, (uint32_t) (uint32_t) -TIMEOUT_COUNT);
        }
        _mark_half_rotation(accum_time_us);
    }
    for (int i = sizeof(_accum_time_us_history) / sizeof(uint32_t) - 1; i > 0; i--) {
        _accum_time_us_history[i] = _accum_time_us_history[i - 1];
    }
    _accum_time_us_history[0] = accum_time_us;
}

void rotation_calc::_mark_half_rotation(uint32_t interval_us)
{
    float rotation_per_second = 1.0e6 / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO / interval_us;
    float hub_radius = TAPE_SPEED_CM_PER_SEC / 2.0 / M_PI / rotation_per_second;
    float hub_rotations = (float) _count / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO;
    if (_count % 10 == 0) {
        printf("--------\r\n");
        printf("interval_us = %d\r\n", (int) interval_us);
        printf("rps = %d\r\n", (int) (rotation_per_second * 1000));
        printf("radius = %d\r\n", (int) (hub_radius * 1000));
        printf("hub rotations = %d\r\n", (int) (hub_rotations * 1000));
    }
    _count++;
}
