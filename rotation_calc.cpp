/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <cstdio>
#include <cmath>

#include "rotation_calc.h"
#include "rotation_term.pio.h"

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"

#define CRP42602Y_PIO __CONCAT(pio, PICO_CRP42602Y_CTRL_PIO)
#define PIO_IRQ_x __CONCAT(__CONCAT(PIO, PICO_CRP42602Y_CTRL_PIO), _IRQ_0)  // e.g. PIO0_IRQ_0

std::map<uint, rotation_calc*> rotation_calc::_inst_map;

// irq handler for PIO
void __isr __time_critical_func(crp42602y_ctrl_pio_irq_handler)()
{
    for (int i = 0; i < 4; i++) {
        if (pio_interrupt_get(CRP42602Y_PIO, i)) {
            pio_interrupt_clear(CRP42602Y_PIO, i);
            rotation_calc* rc = rotation_calc::_inst_map[i];
            rc->irq_callback();
        }
    }
}

static inline uint64_t _micros(void)
{
    return to_us_since_boot(get_absolute_time());
}

rotation_calc::rotation_calc(uint pin_rotation_sens) : _count(0), _sm(0)
{
    // PIO
    while (pio_sm_is_claimed(CRP42602Y_PIO, _sm)) {
        if (++_sm >= 4) panic("all SMs are reserved");
    }
    pio_sm_claim(CRP42602Y_PIO, _sm);

    // PIO_IRQ
    if (!irq_has_shared_handler(PIO_IRQ_x)) {
        irq_add_shared_handler(PIO_IRQ_x, crp42602y_ctrl_pio_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    }

    pio_set_irq0_source_enabled(CRP42602Y_PIO, (enum pio_interrupt_source) ((uint) pis_interrupt0 + _sm), true);   // for IRQ relative
    pio_set_irq1_source_enabled(CRP42602Y_PIO, (enum pio_interrupt_source) ((uint) pis_interrupt0 + _sm), false);  // not use
    pio_interrupt_clear(CRP42602Y_PIO, _sm);
    irq_set_enabled(PIO_IRQ_x, true);

    printf("PIO %d %d\r\n", (int) CRP42602Y_PIO, (int) _sm);
    // PIO
    uint offset = pio_add_program(CRP42602Y_PIO, &rotation_term_program);
    rotation_term_program_init(
        CRP42602Y_PIO,
        _sm,
        offset,
        rotation_term_offset_entry_point,
        rotation_term_program_get_default_config,
        pin_rotation_sens,
        TIMEOUT_COUNT
    );

    _inst_map[_sm] = this;
}

rotation_calc::~rotation_calc()
{
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
    while (pio_sm_get_rx_fifo_level(CRP42602Y_PIO, _sm)) {
        uint32_t val = TIMEOUT_COUNT - pio_sm_get_blocking(CRP42602Y_PIO, _sm);
        printf("val = %d us\r\n", val);
    }
}

void rotation_calc::mark_half_rotation()
{
    uint64_t now_time = _micros();
    uint32_t diff_time;
    if (now_time > _prev_time) {
        diff_time = (uint32_t) (now_time - _prev_time);
    } else {
        diff_time = (uint32_t) (now_time + ((~0LL) - _prev_time) + 1);
    }
    float rotation_per_second = 1.0e6 / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO / diff_time;
    float hub_radius = TAPE_SPEED_CM_PER_SEC / 2.0 / M_PI / rotation_per_second;
    float hub_rotations = (float) _count / NUM_ROTATION_WINGS / ROTATION_GEAR_RATIO;
    if (_count % 10 == 0) {
        printf("diff_time = %d\r\n", (int) diff_time);
        printf("rps = %d\r\n", (int) (rotation_per_second * 1000));
        printf("radius = %d\r\n", (int) (hub_radius * 1000));
        printf("hub rotations = %d\r\n", (int) (hub_rotations * 1000));
    }
    _prev_time = now_time;
    _count++;
}