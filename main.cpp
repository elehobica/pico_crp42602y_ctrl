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

static constexpr uint PIN_LED             = PICO_DEFAULT_LED_PIN;
// CRP42602Y Control Pins
static constexpr uint PIN_SOLENOID_CTRL   = 2;
static constexpr uint PIN_CASSETTE_DETECT = 3;
static constexpr uint PIN_GEAR_STATUS_SW  = 4;
static constexpr uint PIN_ROTATION_SENS   = 5;  // This needs to be PWM_B pin
// Buttons
static constexpr uint PIN_UP_BUTTON     = 18;
static constexpr uint PIN_DOWN_BUTTON   = 19;
static constexpr uint PIN_LEFT_BUTTON   = 20;
static constexpr uint PIN_RIGHT_BUTTON  = 21;
static constexpr uint PIN_CENTER_BUTTON = 22;
static constexpr uint PIN_SET_BUTTON    = 26;
static constexpr uint PIN_RESET_BUTTON  = 27;

static uint pwmSliceNum;
static uint16_t prevRotCount = 0;
volatile static bool rotating = false;

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

Buttons* buttons = nullptr;
crp42602y_ctrl *crp42602y_ctrl0 = nullptr;

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
        printf("stop\r\n");
    }
}

static void playA()
{
    if (crp42602y_ctrl0->send_command(crp42602y_ctrl::PLAY_A_COMMAND)) {
        printf("play A\r\n");
    }
}

static void playB()
{
    if (crp42602y_ctrl0->send_command(crp42602y_ctrl::PLAY_B_COMMAND)) {
        printf("play B\r\n");
    }
}

static void play(bool nonReverse)
{
    bool is_dir_a = crp42602y_ctrl0->is_dir_a();
    if (nonReverse) {
        if (crp42602y_ctrl0->send_command(crp42602y_ctrl::PLAY_COMMAND)) {
            printf("play %c\r\n", is_dir_a ? 'A' : 'B');
        }
    } else {
        if (crp42602y_ctrl0->send_command(crp42602y_ctrl::PLAY_REVERSE_COMMAND)) {
            printf("play %c\r\n", !is_dir_a ? 'A' : 'B');  // opposite to current
        }
    }
}

static void fwd()
{
    bool is_dir_a = crp42602y_ctrl0->is_dir_a();
    if (crp42602y_ctrl0->send_command(crp42602y_ctrl::FWD_COMMAND)) {
        printf("fwd (%s)\r\n", is_dir_a ? "fwd A" : "rwd B");
    }
}

static void rwd()
{
    bool is_dir_a = crp42602y_ctrl0->is_dir_a();
    if (crp42602y_ctrl0->send_command(crp42602y_ctrl::RWD_COMMAND)) {
        printf("rwd (%s)\r\n", is_dir_a ? "rwd A" : "fwd B");
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
        printf("gear error\r\n");
        break;
    case crp42602y_ctrl::ON_CASSETTE_SET:
        printf("cassette set\r\n");
        break;
    case crp42602y_ctrl::ON_CASSETTE_EJECT:
        printf("cassette eject\r\n");
        break;
    case crp42602y_ctrl::ON_STOP:
        printf("stopped\r\n");
        break;
    case crp42602y_ctrl::ON_REVERSE:
        printf("reversed\r\n");
        break;
    default:
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

    // CRP42602Y pins
    gpio_init(PIN_CASSETTE_DETECT);
    gpio_set_dir(PIN_CASSETTE_DETECT, GPIO_IN);
    gpio_pull_up(PIN_CASSETTE_DETECT);

    gpio_init(PIN_GEAR_STATUS_SW);
    gpio_set_dir(PIN_GEAR_STATUS_SW, GPIO_IN);
    gpio_pull_up(PIN_GEAR_STATUS_SW);

    gpio_pull_up(PIN_ROTATION_SENS);

    gpio_init(PIN_SOLENOID_CTRL);
    gpio_put(PIN_SOLENOID_CTRL, 1);  // set default = 1 before output mode
    gpio_set_dir(PIN_SOLENOID_CTRL, GPIO_OUT);

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
    crp42602y_ctrl0 = new crp42602y_ctrl(PIN_CASSETTE_DETECT, PIN_GEAR_STATUS_SW, PIN_ROTATION_SENS, PIN_SOLENOID_CTRL);
    crp42602y_ctrl0->register_callback_all(crp42602y_ctrl_callback);

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
            if (c == 'a') playA();
            if (c == 'b') playB();
            if (c == 'f') fwd();
            if (c == 'r') rwd();
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
                    } else if (strncmp(btnEvent.button_name, "left", 4) == 0) {
                        crp42602y_ctrl::reverse_mode_t reverse_mode = crp42602y_ctrl0->get_reverse_mode();
                        reverse_mode = (crp42602y_ctrl::reverse_mode_t) ((int) reverse_mode + 1);
                        if (reverse_mode == crp42602y_ctrl::__NUM_RVS_MODES__) {
                            reverse_mode = crp42602y_ctrl::RVS_ONE_WAY;
                        }
                        crp42602y_ctrl0->set_reverse_mode(reverse_mode);
                        switch (reverse_mode) {
                        case crp42602y_ctrl::RVS_ONE_WAY:
                            printf("reverse mode: one way\r\n");
                            break;
                        case crp42602y_ctrl::RVS_ONE_ROUND:
                            printf("reverse mode: one round\r\n");
                            break;
                        case crp42602y_ctrl::RVS_INFINITE_ROUND:
                            printf("reverse mode: infinite round\r\n");
                            break;
                        default:
                            break;
                        }
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
