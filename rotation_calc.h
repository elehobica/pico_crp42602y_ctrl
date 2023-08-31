/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

class rotation_calc {
    public:
    rotation_calc();
    virtual ~rotation_calc() {};
    void mark_half_rotation();

    private:
    static constexpr float TAPE_SPEED_CM_PER_SEC = 4.75;
    static constexpr int   NUM_ROTATION_WINGS = 2;
    static constexpr float ROTATION_GEAR_RATIO = 43.0 / 23.0;
    int _count;
    uint64_t _prev_time;
};