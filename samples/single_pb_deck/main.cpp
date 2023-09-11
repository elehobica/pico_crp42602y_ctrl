/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"

#include "Buttons.h"
#include "crp42602y_ctrl.h"
#include "eq_nr.h"
extern "C" {
#include "ssd1306.h"
}

static constexpr uint PIN_LED = PICO_DEFAULT_LED_PIN;

// CRP42602Y control pins
static constexpr uint PIN_SOLENOID_CTRL   = 2;
static constexpr uint PIN_CASSETTE_DETECT = 3;
static constexpr uint PIN_GEAR_STATUS_SW  = 4;
static constexpr uint PIN_ROTATION_SENS   = 5;
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

// SSD1306 pins
static constexpr uint PIN_SSD1306_SDA = 8;
static constexpr uint PIN_SSD1306_SCL = 9;

// Timer & frequency
static repeating_timer_t timer;
static constexpr int INTERVAL_MS_BUTTONS_CHECK = 50;

static uint32_t _count = 0;
//static uint32_t _t = 0;

static bool _has_cassette = false;
static bool _crp42602y_power = true;
static queue_t _callback_queue;
static constexpr int CALLBACK_QUEUE_LENGTH = 16;

static button_t btns_5way_tactile_plus2[] = {
    {"reset",  PIN_RESET_BUTTON,  &Buttons::DEFAULT_BUTTON_SINGLE_CONFIG},
    {"set",    PIN_SET_BUTTON,    &Buttons::DEFAULT_BUTTON_SINGLE_CONFIG},
    {"center", PIN_CENTER_BUTTON, &Buttons::DEFAULT_BUTTON_MULTI_CONFIG},
    {"right",  PIN_RIGHT_BUTTON,  &Buttons::DEFAULT_BUTTON_MULTI_CONFIG},
    {"left",   PIN_LEFT_BUTTON,   &Buttons::DEFAULT_BUTTON_MULTI_CONFIG},
    {"down",   PIN_DOWN_BUTTON,   &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG},
    {"up",     PIN_UP_BUTTON,     &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG}
};

// Instances
Buttons* buttons = nullptr;
crp42602y_ctrl *crp42602y_ctrl0 = nullptr;
crp42602y_counter *crp42602y_counter0 = nullptr;
eq_nr *eq_nr0 = nullptr;
ssd1306_t disp;

/*
 * Font Format for reverse mode
 * <height>, <width>, <additional spacing per char>, 
 * <first ascii char>, <last ascii char>,
 * <data>
 */
const uint8_t font_reverse_mode[] =
{
    16, 16, 1, 0, 2,
    // One Way
    0x00,0x00,0x10,0x08,0x10,0x1C,0x10,0x3E,0x10,0x7F,0x10,0x08,0x10,0x08,0x10,0x08,
    0x10,0x08,0x10,0x08,0x10,0x08,0xFE,0x08,0x7C,0x08,0x38,0x08,0x10,0x08,0x00,0x00,
    // One Round
    0x00,0x00,0x10,0x08,0x10,0x1C,0x10,0x3E,0x10,0x7F,0x10,0x08,0x10,0x08,0x10,0x08,
    0x10,0x08,0x10,0x08,0x10,0x08,0x10,0x08,0x10,0x08,0x20,0x04,0xC0,0x03,0x00,0x00,
    // Infinite Round
    0x00,0x00,0xC0,0x03,0x20,0x04,0x10,0x08,0x10,0x1C,0x10,0x3E,0x10,0x7F,0x10,0x08,
    0x10,0x08,0xFE,0x08,0x7C,0x08,0x38,0x08,0x10,0x08,0x20,0x04,0xC0,0x03,0x00,0x00
};

static inline uint64_t _micros()
{
    return to_us_since_boot(get_absolute_time());
}

static inline uint32_t _millis()
{
    return to_ms_since_boot(get_absolute_time());
}

static void _ssd1306_clear_square(ssd1306_t* p, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    for (uint32_t iy = y; iy < y + height; iy++) {
        for (uint32_t ix = x; ix < x + width; ix++) {
            ssd1306_clear_pixel(p, ix, iy);
        }
    }
}

static void _ssd1306_draw_arrow(ssd1306_t* p, uint32_t x, uint32_t y, bool right_dir)
{
    for (int i = 0; i < 6; i++) {
        uint32_t sx = x + i*1;
        uint32_t sy = y;
        if (right_dir) {
            ssd1306_draw_line(p, sx, sy, sx+8, sy+8);
            ssd1306_draw_line(p, sx, sy+16, sx+8, sy+8);
        } else {
            ssd1306_draw_line(p, sx, sy+8, sx+8, sy);
            ssd1306_draw_line(p, sx, sy+8, sx+8, sy+16);
        }
    }
}

static void _ssd1306_draw_stop_arrow(ssd1306_t* p, bool right_dir)
{
    uint32_t ox = 64-14/2;
    uint32_t oy = 32-8;
    _ssd1306_draw_arrow(p, ox, oy, right_dir);
}

// pos: 0 ~ 15
static void _ssd1306_draw_play_arrow(ssd1306_t* p, bool right_dir, uint32_t pos)
{
    uint32_t ox = 64-14/2;
    uint32_t oy = 32-8;
    uint32_t ofs;
    if (right_dir) {
        ofs = (pos + 8) % 16;
    } else {
        ofs = (24 - pos) % 16;
    }
    _ssd1306_draw_arrow(p, ox + ofs - 8, oy, right_dir);
}

static void _ssd1306_draw_cue_arrow(ssd1306_t* p, bool right_dir, uint32_t pos)
{
    uint32_t ox = 64-22/2;
    uint32_t oy = 32-8;
    uint32_t ofs;
    if (right_dir) {
        ofs = (pos + 8) % 16;
    } else {
        ofs = (24 - pos) % 16;
    }
    for (int i = 0; i < 2; i++) {
        _ssd1306_draw_arrow(p, ox + i*8 + ofs - 8, oy, right_dir);
    }
}

static bool periodic_func(repeating_timer_t *rt)
{
    if (buttons != nullptr) {
        //uint64_t t0 = _micros();
        buttons->scan_periodic();
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
    bool head_dir_is_a = crp42602y_ctrl0->get_head_dir_is_a();
    if (inc) {
        head_dir_is_a = !head_dir_is_a;
        head_dir_is_a = crp42602y_ctrl0->set_head_dir_is_a(head_dir_is_a);
        printf("head dir %c\r\n", head_dir_is_a ? 'A' : 'B');
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
    _ssd1306_clear_square(&disp, 0, 64-16, 16, 16);
    ssd1306_draw_char_with_font(&disp, 0, 64-16, 1, font_reverse_mode, (char) reverse_mode);
    ssd1306_show(&disp);
}

void inc_eq(bool inc = true)
{
    crp42602y_ctrl0->recover_power_from_timeout();
    eq_nr::eq_type_t eq_type = eq_nr0->get_eq_type();
    if (inc) {
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
    const int n = 6;
    const uint32_t sx = 128-6*n;
    const uint32_t sy = 0;
    _ssd1306_clear_square(&disp, sx, sy, 6*n, 8);
    switch (eq_type) {
    case eq_nr::EQ_120US:
        ssd1306_draw_string(&disp, sx, sy, 1, "120 uS");
        break;
    case eq_nr::EQ_70US:
        ssd1306_draw_string(&disp, sx, sy, 1, " 70 uS");
        break;
    default:
        ssd1306_draw_string(&disp, sx, sy, 1, "120 uS");
        break;
    }
    ssd1306_show(&disp);
}

void inc_nr(bool inc = true)
{
    crp42602y_ctrl0->recover_power_from_timeout();
    eq_nr::nr_type_t nr_type = eq_nr0->get_nr_type();
    if (inc) {
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
    const int n = 7;
    const uint32_t sx = 128-6*n;
    const uint32_t sy = 64-8;
    _ssd1306_clear_square(&disp, sx, sy, 6*n, 8);
    switch (nr_type) {
    case eq_nr::NR_OFF:
        ssd1306_draw_string(&disp, sx, sy, 1, " NR OFF");
        break;
    case eq_nr::DOLBY_B:
        ssd1306_draw_string(&disp, sx, sy, 1, "Dolby B");
        break;
    case eq_nr::DOLBY_C:
        ssd1306_draw_string(&disp, sx, sy, 1, "Dolby C");
        break;
    default:
        ssd1306_draw_string(&disp, sx, sy, 1, "NR OFF");
        break;
    }
    ssd1306_show(&disp);
}

void reset_counter()
{
    if (crp42602y_counter0 != nullptr) {
        printf("Reset counter\r\n");
        crp42602y_counter0->reset_counter();
    }
}

void disp_default_contents()
{
    _ssd1306_clear_square(&disp, 0, 8, 128, 8*5);
    if (_has_cassette) {
        _ssd1306_draw_stop_arrow(&disp, crp42602y_ctrl0->get_head_dir_is_a());
    } else {
        ssd1306_draw_string(&disp, 32, 32-4, 1, "NO CASSETTE");
    }

    _ssd1306_clear_square(&disp, 0, 0, 6*6, 8);
    ssd1306_draw_string(&disp, 0, 0, 1, "STOP");
    ssd1306_show(&disp);

    inc_head_dir(false);
    inc_reverse_mode(false);
    inc_eq(false);
    inc_nr(false);
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

    // Pins for SSD1306
    i2c_init(i2c0, 400000);
    gpio_set_function(PIN_SSD1306_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SSD1306_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SSD1306_SDA);
    gpio_pull_up(PIN_SSD1306_SCL);

    // Buttons
    buttons = new Buttons(btns_5way_tactile_plus2, sizeof(btns_5way_tactile_plus2) / sizeof(button_t));

    // CRP42602Y_CTRL
    queue_init(&_callback_queue, sizeof(crp42602y_ctrl::callback_type_t), CALLBACK_QUEUE_LENGTH);
    crp42602y_ctrl0 = new crp42602y_ctrl(PIN_CASSETTE_DETECT, PIN_GEAR_STATUS_SW, PIN_ROTATION_SENS, PIN_SOLENOID_CTRL, PIN_POWER_CTRL);
    crp42602y_ctrl0->register_callback_all(crp42602y_callback);
    crp42602y_counter0 = crp42602y_ctrl0->get_counter_inst();

    // EQ_NR
    eq_nr0 = new eq_nr(PIN_EQ_CTRL, PIN_NR_CTRL0, PIN_NR_CTRL1);

    // SSD1306
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
    ssd1306_clear(&disp);
    ssd1306_show(&disp);
    disp_default_contents();

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

    uint32_t prev_disp_time = 0;
    int disp_count = 0;
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
            if (c == 'e') inc_eq();
            if (c == 'n') inc_nr();
            if (c == 'c') reset_counter();
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
                        fast_forward();
                    } else if (strncmp(btnEvent.button_name, "up", 2) == 0) {
                        rewind();
                    } else if (strncmp(btnEvent.button_name, "right", 5) == 0) {
                        inc_head_dir(_crp42602y_power && _has_cassette);
                    } else if (strncmp(btnEvent.button_name, "left", 4) == 0) {
                        inc_reverse_mode(_crp42602y_power);
                    } else if (strncmp(btnEvent.button_name, "reset", 5) == 0) {
                        inc_eq(_crp42602y_power);
                    } else if (strncmp(btnEvent.button_name, "set", 3) == 0) {
                        inc_nr(_crp42602y_power);
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
                if (strncmp(btnEvent.button_name, "left", 4) == 0) {
                    reset_counter();
                }
                break;
            case EVT_LONG_LONG:
                //printf("%s: LongLong\r\n", btnEvent.button_name);
                break;
            default:
                break;
            }
        }

        // Process callback
        crp42602y_ctrl::callback_type_t callback_type;
        while (crp42602y_get_callback(&callback_type)) {
            switch (callback_type) {
            case crp42602y_ctrl::ON_GEAR_ERROR:
                printf("Gear error\r\n");
                prev_disp_time = 0;
                break;
            case crp42602y_ctrl::ON_COMMAND_FIFO_OVERFLOW:
                printf("Command FIFO overflow\r\n");
                prev_disp_time = 0;
                break;
            case crp42602y_ctrl::ON_COUNTER_FIFO_OVERFLOW:
                printf("Counter FIFO overflow\r\n");
                prev_disp_time = 0;
                break;
            case crp42602y_ctrl::ON_CASSETTE_SET:
                printf("Cassette set\r\n");
                _has_cassette = true;
                prev_disp_time = 0;
                crp42602y_ctrl0->recover_power_from_timeout();
                break;
            case crp42602y_ctrl::ON_CASSETTE_EJECT:
                printf("Cassette eject\r\n");
                _has_cassette = false;
                prev_disp_time = 0;
                break;
            case crp42602y_ctrl::ON_STOP:
                printf("Stop\r\n");
                _ssd1306_clear_square(&disp, 0, 0, 6*6, 8);
                ssd1306_draw_string(&disp, 0, 0, 1, "STOP");
                ssd1306_show(&disp);
                prev_disp_time = 0;
                break;
            case crp42602y_ctrl::ON_PLAY:
                _ssd1306_clear_square(&disp, 0, 0, 6*6, 8);
                if (crp42602y_ctrl0->get_head_dir_is_a()) {
                    printf("Play A\r\n");
                    ssd1306_draw_string(&disp, 0, 0, 1, "PLAY A");
                } else {
                    printf("Play B\r\n");
                    ssd1306_draw_string(&disp, 0, 0, 1, "PLAY B");
                }
                ssd1306_show(&disp);
                prev_disp_time = 0;
                break;
            case crp42602y_ctrl::ON_CUE:
                _ssd1306_clear_square(&disp, 0, 0, 6*6, 8);
                if (crp42602y_ctrl0->get_cue_dir_is_a()) {
                    printf("FF\r\n");
                    ssd1306_draw_string(&disp, 0, 0, 1, "FF");
                } else {
                    printf("REW\r\n");
                    ssd1306_draw_string(&disp, 0, 0, 1, "REW");
                }
                ssd1306_show(&disp);
                prev_disp_time = 0;
                break;
            case crp42602y_ctrl::ON_REVERSE:
                printf("Reversed\r\n");
                _ssd1306_clear_square(&disp, 0, 0, 6*6, 8);
                if (crp42602y_ctrl0->get_head_dir_is_a()) {
                    printf("Play A\r\n");
                    ssd1306_draw_string(&disp, 0, 0, 1, "Play A");
                } else {
                    printf("Play B\r\n");
                    ssd1306_draw_string(&disp, 0, 0, 1, "Play B");
                }
                ssd1306_show(&disp);
                prev_disp_time = 0;
                break;
            case crp42602y_ctrl::ON_TIMEOUT_POWER_OFF:
                printf("Power off\r\n");
                ssd1306_clear(&disp);
                ssd1306_show(&disp);
                _crp42602y_power = false;
                prev_disp_time = 0;
                break;
            case crp42602y_ctrl::ON_RECOVER_POWER_FROM_TIMEOUT:
                printf("Power recover\r\n");
                disp_default_contents();
                _crp42602y_power = true;
                prev_disp_time = 0;
                break;
            }
        }

        if (now_time - prev_disp_time > 50) {
            if (_crp42602y_power) {
                // Image Display
                _ssd1306_clear_square(&disp, 0, 16, 128, 8*4);
                if (!_has_cassette) {
                    ssd1306_draw_string(&disp, 32, 32-4, 1, "NO CASSETTE");
                    disp_count = 0;
                } else {
                    if (crp42602y_ctrl0->is_playing()) {
                        uint32_t pos = disp_count/4 % 16;
                        _ssd1306_draw_play_arrow(&disp, crp42602y_ctrl0->get_head_dir_is_a(), pos);
                        disp_count++;
                    } else if (crp42602y_ctrl0->is_cueing()) {
                        uint32_t pos = disp_count % 16;
                        _ssd1306_draw_cue_arrow(&disp, crp42602y_ctrl0->get_cue_dir_is_a(), pos);
                        disp_count++;
                    } else {  // STOP
                        _ssd1306_draw_stop_arrow(&disp, crp42602y_ctrl0->get_head_dir_is_a());
                        disp_count = 0;
                    }
                }
                // Counter
                _ssd1306_clear_square(&disp, 6*6, 64-8, 6*7, 8);
                float counter_sec_f = crp42602y_counter0->get_counter();
                if (crp42602y_counter0->get_counter_state() == crp42602y_counter::UNDETERMINED) {
                    ssd1306_draw_string(&disp, 6*6, 64-8, 1, "  --:--");
                } else if ((!crp42602y_ctrl0->is_playing() && !crp42602y_ctrl0->is_cueing()) ||
                           crp42602y_ctrl0->is_playing() ||
                           (crp42602y_ctrl0->is_cueing() && crp42602y_counter0->get_counter_state() != crp42602y_counter::PLAY_ONLY || (now_time / 125) % 8 > 0)) {
                            // blick counter during estimation under cueing
                    int counter_sec = (int) counter_sec_f;
                    int counter_min = counter_sec / 60;
                    char str[16];
                    if (counter_sec < 0 && counter_min == 0) {
                        sprintf(str, "  -0:%02d", abs(counter_sec) % 60);
                    } else {
                        sprintf(str, "%4d:%02d", counter_min, abs(counter_sec) % 60);
                    }
                    ssd1306_draw_string(&disp, 6*6, 64-8, 1, str);
                }
                ssd1306_show(&disp);
            } else {
                disp_count = 0;
            }
            prev_disp_time = now_time;
        }
    }

    return 0;
}
