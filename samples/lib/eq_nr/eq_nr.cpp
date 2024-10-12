/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "eq_nr.h"

eq_nr::eq_nr(uint pin_eq_ctrl, uint pin_nr_ctrl0, uint pin_nr_ctrl1, uint pin_eq_mute) :
    _pin_eq_ctrl(pin_eq_ctrl), _pin_nr_ctrl0(pin_nr_ctrl0), _pin_nr_ctrl1(pin_nr_ctrl1), _pin_eq_mute(pin_eq_mute),
    _eq_type(EQ_120US), _nr_type(NR_OFF)
{
    gpio_init(_pin_eq_ctrl);
    set_eq_type(_eq_type);
    gpio_set_dir(_pin_eq_ctrl, GPIO_OUT);

    gpio_init(_pin_nr_ctrl0);
    gpio_init(_pin_nr_ctrl1);
    set_nr_type(_nr_type);
    gpio_set_dir(_pin_nr_ctrl0, GPIO_OUT);
    gpio_set_dir(_pin_nr_ctrl1, GPIO_OUT);

    if (_pin_eq_mute > 0) {
      gpio_init(_pin_eq_mute);
      set_mute(true);
      gpio_set_dir(_pin_eq_mute, GPIO_OUT);
    }
}

void eq_nr::set_eq_type(eq_type_t type)
{
    _eq_type = type;
    gpio_put(_pin_eq_ctrl, _eq_type == EQ_70US);
}

eq_nr::eq_type_t eq_nr::get_eq_type() const
{
    return _eq_type;
}

void eq_nr::set_nr_type(nr_type_t type)
{
    _nr_type = type;
    switch (_nr_type) {
    case NR_OFF:
        gpio_put(_pin_nr_ctrl0, false);
        gpio_put(_pin_nr_ctrl1, true);
        break;
    case DOLBY_B:
        gpio_put(_pin_nr_ctrl0, false);
        gpio_put(_pin_nr_ctrl1, false);
        break;
    case DOLBY_C:
        gpio_put(_pin_nr_ctrl0, true);
        gpio_put(_pin_nr_ctrl1, false);
        break;
    default:
        gpio_put(_pin_nr_ctrl0, false);
        gpio_put(_pin_nr_ctrl1, true);
        break;
    }
}

eq_nr::nr_type_t eq_nr::get_nr_type() const
{
    return _nr_type;
}

void eq_nr::set_mute(bool flag)
{
    _is_muted = flag;
    if (_pin_eq_mute > 0) {
        gpio_put(_pin_eq_mute, flag);
    }
}

bool eq_nr::get_mute() const
{
    return _is_muted;
}
