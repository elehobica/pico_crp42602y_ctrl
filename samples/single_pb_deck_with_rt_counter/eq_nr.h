/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#pragma once

#include "pico/stdlib.h"

class eq_nr {
    public:
    typedef enum _eq_type_t {
        EQ_120US = 0,
        EQ_70US,
        __NUM_EQ_TYPES__
    } eq_type_t;
    typedef enum _nr_type_t {
        NR_OFF = 0,
        DOLBY_B,
        DOLBY_C,
        __NUM_NR_TYPES__
    } nr_type_t;

    eq_nr(uint pin_eq_ctrl, uint pin_nr_ctrl0, uint pin_nr_ctrl1);
    virtual ~eq_nr() {}
    void set_eq_type(eq_type_t type);
    eq_type_t get_eq_type() const;
    void set_nr_type(nr_type_t type);
    nr_type_t get_nr_type() const;

    private:
    uint _pin_eq_ctrl;
    uint _pin_nr_ctrl0;
    uint _pin_nr_ctrl1;
    eq_type_t _eq_type;
    nr_type_t _nr_type;
};

