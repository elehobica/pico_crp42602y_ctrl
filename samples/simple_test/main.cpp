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
#include "pico/util/queue.h"

#include "crp42602y_ctrl.h"

static constexpr uint PIN_LED = PICO_DEFAULT_LED_PIN;

// CRP42602Y control pins
static constexpr uint PIN_SOLENOID_CTRL   = 2;
static constexpr uint PIN_CASSETTE_DETECT = 3;
static constexpr uint PIN_GEAR_STATUS_SW  = 4;
static constexpr uint PIN_ROTATION_SENS   = 5;  // This needs to be PWM_B pin
static constexpr uint PIN_POWER_CTRL      = 6;

// ADC Timer & frequency
static repeating_timer_t timer;
static constexpr int INTERVAL_MS_CRP42602Y_CTRL_FUNC = 100;

static uint32_t _count = 0;
//static uint32_t _t = 0;

static bool _has_cassette = false;
static bool _crp42602y_power = true;
static queue_t _callback_queue;
static constexpr int CALLBACK_QUEUE_LENGTH = 16;

// Instances
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
    if (crp42602y_ctrl0 != nullptr) {
        //uint64_t t0 = _micros();
        crp42602y_ctrl0->periodic_func_100ms();
        //_t = (uint32_t) (_micros() - t0);
    }
    _count++;
    return true; // keep repeating
}

static void stop()
{
    crp42602y_ctrl0->send_command(crp42602y_ctrl::STOP_COMMAND);
}

static void play(bool nonReverse)
{
    if (nonReverse) {
        crp42602y_ctrl0->send_command(crp42602y_ctrl::PLAY_COMMAND);
    } else {
        crp42602y_ctrl0->send_command(crp42602y_ctrl::PLAY_REVERSE_COMMAND);
    }
}

static void fast_forward()
{
    crp42602y_ctrl0->send_command(crp42602y_ctrl::FF_COMMAND);
}

static void rewind()
{
    crp42602y_ctrl0->send_command(crp42602y_ctrl::REW_COMMAND);
}

static void crp42602y_process()
{
    stop();
    while (true) {
        crp42602y_ctrl0->process_loop();
    }
}

void crp42602y_callback(const crp42602y_ctrl::callback_type_t callback_type)
{
    if (!queue_try_add(&_callback_queue, &callback_type)) {
        printf("ERROR: _callback_queue is full\r\n");
    }
}

bool crp42602y_get_callback(crp42602y_ctrl::callback_type_t* callback_type)
{
    if (queue_get_level(&_callback_queue) > 0) {
        queue_remove_blocking(&_callback_queue, callback_type);
        return true;
    } else {
        return false;
    }
}

void inc_head_dir(bool inc = true)
{
    crp42602y_ctrl0->recover_power_from_timeout();
    bool head_dir_a = crp42602y_ctrl0->get_head_dir_a();
    if (inc) {
        head_dir_a = !head_dir_a;
        head_dir_a = crp42602y_ctrl0->set_head_dir_a(head_dir_a);
        printf("head dir %c\r\n", head_dir_a ? 'A' : 'B');
    }
}

void inc_reverse_mode(bool inc = true)
{
    crp42602y_ctrl0->recover_power_from_timeout();
    crp42602y_ctrl::reverse_mode_t reverse_mode = crp42602y_ctrl0->get_reverse_mode();
    if (inc) {
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

    // CRP42602Y_CTRL
    queue_init(&_callback_queue, sizeof(crp42602y_ctrl::callback_type_t), CALLBACK_QUEUE_LENGTH);
    crp42602y_ctrl0 = new crp42602y_ctrl(PIN_CASSETTE_DETECT, PIN_GEAR_STATUS_SW, PIN_ROTATION_SENS, PIN_SOLENOID_CTRL, PIN_POWER_CTRL);
    crp42602y_ctrl0->register_callback_all(crp42602y_callback);

    // negative timeout means exact delay (rather than delay between callbacks)
    if (!add_repeating_timer_us(-INTERVAL_MS_CRP42602Y_CTRL_FUNC * 1000, periodic_func, nullptr, &timer)) {
        printf("Failed to add timer\n");
        return 0;
    }

    printf("CRP42602Y control started\r\n");

    // Core1 runs CRP62602Y process
    multicore_reset_core1();
    multicore_launch_core1(crp42602y_process);

    while (true) {
        uint32_t now_time = _millis();

        // Serial I/F
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == 's') stop();
            if (c == 'p') play(true);
            if (c == 'q') play(false);
            if (c == 'f') fast_forward();
            if (c == 'r') rewind();
            if (c == 'd') inc_head_dir();
            if (c == 'v') inc_reverse_mode();
        }

        // Process callback
        crp42602y_ctrl::callback_type_t callback_type;
        while (crp42602y_get_callback(&callback_type)) {
            switch (callback_type) {
            case crp42602y_ctrl::ON_GEAR_ERROR:
                printf("Gear error\r\n");
                break;
            case crp42602y_ctrl::ON_COMMAND_FIFO_OVERFLOW:
                printf("Command FIFO overflow\r\n");
                break;
            case crp42602y_ctrl::ON_CASSETTE_SET:
                printf("Cassette set\r\n");
                _has_cassette = true;
                crp42602y_ctrl0->recover_power_from_timeout();
                break;
            case crp42602y_ctrl::ON_CASSETTE_EJECT:
                printf("Cassette eject\r\n");
                _has_cassette = false;
                break;
            case crp42602y_ctrl::ON_STOP:
                printf("Stop\r\n");
                break;
            case crp42602y_ctrl::ON_PLAY:
                if (crp42602y_ctrl0->get_head_dir_a()) {
                    printf("Play A\r\n");
                } else {
                    printf("Play B\r\n");
                }
                break;
            case crp42602y_ctrl::ON_CUE:
                if (crp42602y_ctrl0->get_cue_dir_a()) {
                    printf("FF\r\n");
                } else {
                    printf("REW\r\n");
                }
                break;
            case crp42602y_ctrl::ON_REVERSE:
                printf("Reversed\r\n");
                if (crp42602y_ctrl0->get_head_dir_a()) {
                    printf("Play A\r\n");
                } else {
                    printf("Play B\r\n");
                }
                break;
            case crp42602y_ctrl::ON_TIMEOUT_POWER_OFF:
                printf("Power off\r\n");
                _crp42602y_power = false;
                break;
            case crp42602y_ctrl::ON_RECOVER_POWER_FROM_TIMEOUT:
                printf("Power recover\r\n");
                _crp42602y_power = true;
                break;
            }
        }
    }

    return 0;
}
