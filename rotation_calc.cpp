/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <cstdio>
#include <cmath>

#include "rotation_calc.h"

#include "pico/stdlib.h"

static inline uint64_t _micros(void)
{
    return to_us_since_boot(get_absolute_time());
}

rotation_calc::rotation_calc() : _count(0)
{

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