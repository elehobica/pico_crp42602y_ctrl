/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pwm.h"

#include "Buttons.h"
#include "crp42602y_ctrl.h"
#include "eq_nr.h"

static constexpr uint PIN_LED             = PICO_DEFAULT_LED_PIN;
// CRP42602Y control pins
static constexpr uint PIN_SOLENOID_CTRL   = 2;
static constexpr uint PIN_CASSETTE_DETECT = 3;
static constexpr uint PIN_GEAR_STATUS_SW  = 4;
static constexpr uint PIN_ROTATION_SENS   = 5;  // This needs to be PWM_B pin
static constexpr uint PIN_POWER_CTRL      = 6;
// EQ NR control pins
static constexpr uint PIN_EQ_CTRL  = 7;
static constexpr uint PIN_NR_CTRL0 = 10;
static constexpr uint PIN_NR_CTRL1 = 11;

// Buttons pins
static constexpr uint PIN_UP_BUTTON     = 18;
static constexpr uint PIN_DOWN_BUTTON   = 19;
static constexpr uint PIN_LEFT_BUTTON   = 20;
static constexpr uint PIN_RIGHT_BUTTON  = 21;
static constexpr uint PIN_CENTER_BUTTON = 22;
static constexpr uint PIN_SET_BUTTON    = 26;
static constexpr uint PIN_RESET_BUTTON  = 27;

// ADC Timer & frequency
static repeating_timer_t timer;
static constexpr int INTERVAL_MS_BUTTONS_CHECK = 50;
static constexpr int INTERVAL_MS_CRP4260Y_CTRL_FUNC = 100;  // > INTERVAL_MS_BUTTONS_CHECK

static uint32_t count = 0;
static uint32_t t = 0;

static button_t btns_5way_tactile_plus2[] = {
    {"reset",  PIN_RESET_BUTTON,  &Buttons::DEFAULT_BUTTON_SINGLE_CONFIG},
    {"set",    PIN_SET_BUTTON,    &Buttons::DEFAULT_BUTTON_SINGLE_CONFIG},
    {"center", PIN_CENTER_BUTTON, &Buttons::DEFAULT_BUTTON_MULTI_CONFIG},
    {"right",  PIN_RIGHT_BUTTON,  &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG},
    {"left",   PIN_LEFT_BUTTON,   &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG},
    {"down",   PIN_DOWN_BUTTON,   &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG},
    {"up",     PIN_UP_BUTTON,     &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG}
};

// Instances
Buttons* buttons = nullptr;
crp42602y_ctrl *crp42602y_ctrl0 = nullptr;
eq_nr *eq_nr0 = nullptr;

static inline uint64_t _micros(void)
{
    return to_us_since_boot(get_absolute_time());
}

static inline uint32_t _millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

static bool periodic_func(repeating_timer_t *rt)
{
    if (buttons != nullptr) {
        //uint64_t t0 = _micros();
        buttons->scan_periodic();
        //t = (uint32_t) (_micros() - t0);
    }
    if (count % (INTERVAL_MS_CRP4260Y_CTRL_FUNC / INTERVAL_MS_BUTTONS_CHECK) == 0) {
        if (crp42602y_ctrl0 != nullptr) {
            //uint64_t t0 = _micros();
            crp42602y_ctrl0->periodic_func_100ms();
            //t = (uint32_t) (_micros() - t0);
        }
    }
    count++;
    return true; // keep repeating
}

static void stop()
{
    if (crp42602y_ctrl0->send_command(crp42602y_ctrl::STOP_COMMAND)) {
        printf("Stop\r\n");
    }
}

static void play(bool nonReverse)
{
    bool head_dir_a = crp42602y_ctrl0->get_head_dir_a();
    if (nonReverse) {
        if (crp42602y_ctrl0->send_command(crp42602y_ctrl::PLAY_COMMAND)) {
            printf("Play %c\r\n", head_dir_a ? 'A' : 'B');
        }
    } else {
        if (crp42602y_ctrl0->send_command(crp42602y_ctrl::PLAY_REVERSE_COMMAND)) {
            printf("Play %c\r\n", !head_dir_a ? 'A' : 'B');  // opposite to current
        }
    }
}

static void fwd()
{
    bool head_dir_a = crp42602y_ctrl0->get_head_dir_a();
    if (crp42602y_ctrl0->send_command(crp42602y_ctrl::FWD_COMMAND)) {
        printf("Fwd (%s)\r\n", head_dir_a ? "fwd A" : "rwd B");
    }
}

static void rwd()
{
    bool head_dir_a = crp42602y_ctrl0->get_head_dir_a();
    if (crp42602y_ctrl0->send_command(crp42602y_ctrl::RWD_COMMAND)) {
        printf("Rwd (%s)\r\n", head_dir_a ? "rwd A" : "fwd B");
    }
}

static void crp42602y_process()
{
    stop();
    while (true) {
        crp42602y_ctrl0->process_loop();
    }
}

void crp42602y_ctrl_callback(const crp42602y_ctrl::callback_type_t callback_type)
{
    switch (callback_type) {
    case crp42602y_ctrl::ON_GEAR_ERROR:
        printf("Gear error\r\n");
        break;
    case crp42602y_ctrl::ON_CASSETTE_SET:
        printf("Cassette set\r\n");
        break;
    case crp42602y_ctrl::ON_CASSETTE_EJECT:
        printf("Cassette eject\r\n");
        break;
    case crp42602y_ctrl::ON_STOP:
        printf("Stopped\r\n");
        break;
    case crp42602y_ctrl::ON_REVERSE:
        printf("Reversed\r\n");
        break;
    case crp42602y_ctrl::ON_TIMEOUT_POWER_OFF:
        printf("Power off\r\n");
        break;
    default:
        break;
    }
}

void set_head_dir()
{
    bool head_dir_a = crp42602y_ctrl0->get_head_dir_a();
    head_dir_a = !head_dir_a;
    head_dir_a = crp42602y_ctrl0->set_head_dir_a(head_dir_a);
    printf("head dir %c\r\n", head_dir_a ? 'A' : 'B');
}

void set_reverse_mode()
{
    crp42602y_ctrl::reverse_mode_t reverse_mode = crp42602y_ctrl0->get_reverse_mode();
    reverse_mode = (crp42602y_ctrl::reverse_mode_t) ((int) reverse_mode + 1);
    if (reverse_mode >= crp42602y_ctrl::__NUM_RVS_MODES__) {
        reverse_mode = crp42602y_ctrl::RVS_ONE_WAY;
    }
    crp42602y_ctrl0->set_reverse_mode(reverse_mode);
    switch (reverse_mode) {
    case crp42602y_ctrl::RVS_ONE_WAY:
        printf("Reverse mode: One way\r\n");
        break;
    case crp42602y_ctrl::RVS_ONE_ROUND:
        printf("Reverse mode: One round\r\n");
        break;
    case crp42602y_ctrl::RVS_INFINITE_ROUND:
        printf("Reverse mode: Infinite round\r\n");
        break;
    default:
        break;
    }
}

void set_eq()
{
    eq_nr::eq_type_t eq_type = eq_nr0->get_eq_type();
    eq_type = (eq_nr::eq_type_t) ((int) eq_type + 1);
    if (eq_type >= eq_nr::__NUM_EQ_TYPES__) {
        eq_type = eq_nr::EQ_120US;
    }
    eq_nr0->set_eq_type(eq_type);
    switch (eq_type) {
    case eq_nr::EQ_120US:
        printf("EQ: 120us\r\n");
        break;
    case eq_nr::EQ_70US:
        printf("EQ: 70us\r\n");
        break;
    default:
        printf("EQ: 120us\r\n");
        break;
    }
}

void set_nr()
{
    eq_nr::nr_type_t nr_type = eq_nr0->get_nr_type();
    nr_type = (eq_nr::nr_type_t) ((int) nr_type + 1);
    if (nr_type >= eq_nr::__NUM_NR_TYPES__) {
        nr_type = eq_nr::NR_OFF;
    }
    eq_nr0->set_nr_type(nr_type);
    switch (nr_type) {
    case eq_nr::NR_OFF:
        printf("NR: OFF\r\n");
        break;
    case eq_nr::DOLBY_B:
        printf("NR: Dolby B\r\n");
        break;
    case eq_nr::DOLBY_C:
        printf("NR: Dolby C\r\n");
        break;
    default:
        printf("NR: OFF\r\n");
        break;
    }
}

int main()
{
    stdio_init_all();

    // GPIO settings
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, 0);

    // CRP42602Y pins pull-up (GPIO mode is cared in the library)
    gpio_pull_up(PIN_CASSETTE_DETECT);
    gpio_pull_up(PIN_GEAR_STATUS_SW);
    gpio_pull_up(PIN_ROTATION_SENS);

    // Pins for Buttons
    for (int i = 0; i < sizeof(btns_5way_tactile_plus2) / sizeof(button_t); i++) {
        button_t* button = &btns_5way_tactile_plus2[i];
        gpio_init(button->pin);
        gpio_set_dir(button->pin, GPIO_IN);
        gpio_pull_up(button->pin);
    }

    // Buttons
    buttons = new Buttons(btns_5way_tactile_plus2, sizeof(btns_5way_tactile_plus2) / sizeof(button_t));

    // CRP42602Y_CTRL
    crp42602y_ctrl0 = new crp42602y_ctrl(PIN_CASSETTE_DETECT, PIN_GEAR_STATUS_SW, PIN_ROTATION_SENS, PIN_SOLENOID_CTRL, PIN_POWER_CTRL);
    crp42602y_ctrl0->register_callback_all(crp42602y_ctrl_callback);

    // EQ_NR
    eq_nr0 = new eq_nr(PIN_EQ_CTRL, PIN_NR_CTRL0, PIN_NR_CTRL1);

    // negative timeout means exact delay (rather than delay between callbacks)
    if (!add_repeating_timer_us(-INTERVAL_MS_BUTTONS_CHECK * 1000, periodic_func, nullptr, &timer)) {
        printf("Failed to add timer\n");
        return 0;
    }

    printf("CRP42602Y control started\r\n");

    // Core1 runs CRP62602Y process
    multicore_reset_core1();
    multicore_launch_core1(crp42602y_process);

    // Core0 handles user interface
    button_event_t btnEvent;

    while (true) {
        // Serial I/F
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == 's') stop();
            if (c == 'p') play(true);
            if (c == 'q') play(false);
            if (c == 'f') fwd();
            if (c == 'r') rwd();
            if (c == 'd') set_head_dir();
            if (c == 'v') set_reverse_mode();
            if (c == 'e') set_eq();
            if (c == 'n') set_nr();
        }

        // Button I/F
        if (buttons->get_button_event(&btnEvent)) {
            switch (btnEvent.type) {
            case EVT_SINGLE:
                if (btnEvent.repeat_count > 0) {
                    //printf("%s: 1 (Repeated %d)\r\n", btnEvent.button_name, btnEvent.repeat_count);
                } else {
                    //printf("%s: 1\r\n", btnEvent.button_name);
                    if (strncmp(btnEvent.button_name, "center", 6) == 0) {
                        if (crp42602y_ctrl0->is_playing() || crp42602y_ctrl0->is_cueing()) {
                            stop();
                        } else {
                            play(true);
                        }
                    } else if (strncmp(btnEvent.button_name, "down", 4) == 0) {
                        fwd();
                    } else if (strncmp(btnEvent.button_name, "up", 2) == 0) {
                        rwd();
                    } else if (strncmp(btnEvent.button_name, "right", 5) == 0) {
                        set_head_dir();
                    } else if (strncmp(btnEvent.button_name, "left", 4) == 0) {
                        set_reverse_mode();
                    } else if (strncmp(btnEvent.button_name, "reset", 5) == 0) {
                        set_eq();
                    } else if (strncmp(btnEvent.button_name, "set", 3) == 0) {
                        set_nr();
                    }
                }
                break;
            case EVT_MULTI:
                //printf("%s: %d\r\n", btnEvent.button_name, btnEvent.click_count);
                if (strncmp(btnEvent.button_name, "center", 6) == 0 && btnEvent.click_count == 2) {
                    play(false);
                }
                break;
            case EVT_LONG:
                //printf("%s: Long\r\n", btnEvent.button_name);
                break;
            case EVT_LONG_LONG:
                //printf("%s: LongLong\r\n", btnEvent.button_name);
                break;
            default:
                break;
            }
        }
    }

    return 0;
}
