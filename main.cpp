/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "Buttons.h"

static constexpr uint PIN_LED             = PICO_DEFAULT_LED_PIN;
// CRP42602Y Control Pins
static constexpr uint PIN_SOLENOID_CTRL   = 2;
static constexpr uint PIN_CASSETTE_DETECT = 3;
static constexpr uint PIN_FUNC_STATUS_SW  = 4;
static constexpr uint PIN_ROTATION_SENS   = 5;  // This needs to be PWM_B pin
// Buttons
static constexpr uint PIN_DOWN_BUTTON   = 18;
static constexpr uint PIN_UP_BUTTON     = 19;
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
static constexpr int INTERVAL_MS_ROTATION_SENS_CHECK = 1000;  // > INTERVAL_MS_BUTTONS_CHECK

static uint32_t scanCnt = 0;
static uint32_t t = 0;

static button_t btns_5way_tactile_plus2[] = {
    {"reset",  PIN_RESET_BUTTON,  &Buttons::DEFAULT_BUTTON_SINGLE_CONFIG},
    {"set",    PIN_SET_BUTTON,    &Buttons::DEFAULT_BUTTON_SINGLE_CONFIG},
    {"center", PIN_CENTER_BUTTON, &Buttons::DEFAULT_BUTTON_MULTI_CONFIG},
    {"left",   PIN_RIGHT_BUTTON,  &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG},
    {"right",  PIN_LEFT_BUTTON,   &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG},
    {"up",     PIN_DOWN_BUTTON,   &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG},
    {"down",   PIN_UP_BUTTON,     &Buttons::DEFAULT_BUTTON_SINGLE_REPEAT_CONFIG}
};

Buttons* buttons = nullptr;

static bool _playDirA = true;

static inline uint64_t _micros(void)
{
    return to_us_since_boot(get_absolute_time());
}

static inline uint32_t _millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

static bool tcRotationSensCheck(repeating_timer_t *rt) {
    gpio_put(PIN_LED, !gpio_get(PIN_LED));
    uint16_t rotCount = pwm_get_counter(pwmSliceNum);
    uint16_t rotDiff;
    if (rotCount < prevRotCount) {
        rotDiff = (uint16_t) ((1U<<16) + rotCount - prevRotCount);
    } else {
        rotDiff = rotCount - prevRotCount;
    }
    rotating = rotDiff > 0;
    //printf("pwm counter = %d, rotating = %d\r\n", (int) rotCount, rotating ? 1 : 0);
    prevRotCount = rotCount;
    return true; // keep repeating
}

static bool scanButtons(repeating_timer_t *rt) {
    if (buttons != nullptr) {
        uint64_t t0 = _micros();
        buttons->scan_periodic();
        t = (uint32_t) (_micros() - t0);
    }

    if (scanCnt % (INTERVAL_MS_ROTATION_SENS_CHECK / INTERVAL_MS_BUTTONS_CHECK) == 0) {
        tcRotationSensCheck(rt);
    }

    scanCnt++;
    return true; // keep repeating
}

void ctrlSolenoid(bool flag)
{
    if (flag) {
        gpio_put(PIN_SOLENOID_CTRL, 0);
    } else {
        gpio_put(PIN_SOLENOID_CTRL, 1);
    }
}

bool isGearInFunc()
{
    return !gpio_get(PIN_FUNC_STATUS_SW);
}

bool hasCassette()
{
    return !gpio_get(PIN_CASSETTE_DETECT);
}

void funcSequence(bool pinchDirA, bool headPosPlay, bool reelFwd)
{
    // Function sequence has 190 degree of function gear to roll in 400 ms
    // Timing definitions (milliseconds) (All values are set experimentally)
    constexpr uint32_t tInitS    = 0;          // Unhook the function gear
    constexpr uint32_t tInitE    = 20;
    constexpr uint32_t tPinchS   = tInitE;     // Term to determine pinch roller and head direction
    constexpr uint32_t tPinchE   = 100;
    constexpr uint32_t tHeadPosS = 150;        // Term to determine head play / evacuate position
    constexpr uint32_t tHeadPosE = 300;
    constexpr uint32_t tReelS    = tHeadPosE;  // Term to determine reel direction
    constexpr uint32_t tReelE    = 400;

    // Be careful about the consistency of pinch roller direction and reel direction,
    //  otherwise they could pull to opposite directions and give unexpected extension stress to the tape

    ctrlSolenoid(true);
    sleep_ms(tInitE);
    ctrlSolenoid(!pinchDirA);
    sleep_ms(tPinchE - tInitE);
    ctrlSolenoid(false);
    sleep_ms(tHeadPosS - tPinchE);
    ctrlSolenoid(headPosPlay);
    sleep_ms(tHeadPosE - tHeadPosS);
    ctrlSolenoid(reelFwd);
    sleep_ms(tReelE - tHeadPosE);
    ctrlSolenoid(false);
}

void returnSequence()
{
    // Return sequence has (360 - 190) degree of function gear,
    //  which is needed to take another function when the gear is already in function position
    //  it is supposed to take 360 ms
    ctrlSolenoid(true);
    sleep_ms(20);
    ctrlSolenoid(false);
    sleep_ms(340);
    sleep_ms(20);  // additional margin
}

void stop()
{
    if (isGearInFunc()) {
        printf("stop\r\n");
        returnSequence();
    }
}

void playA()
{
    if (isGearInFunc()) returnSequence();
    if (!hasCassette()) {
        printf("no cassette\r\n");
        return;
    }
    printf("play A\r\n");
    funcSequence(true, true, true);
    _playDirA = true;
}

void playB()
{
    if (isGearInFunc()) returnSequence();
    if (!hasCassette()) {
        printf("no cassette\r\n");
        return;
    }
    printf("play B\r\n");
    funcSequence(false, true, false);
    _playDirA = false;
}

void play(bool nonReverse)
{
    if (_playDirA ^ !nonReverse) {
        playA();
    } else {
        playB();
    }
}

void fwd()
{
    // Evacuate head, however the head direction still matters for which side the head is tracing,
    //  therefore, previous head direction is preserved and it may mean fwd A or rwd B
    if (isGearInFunc()) returnSequence();
    if (!hasCassette()) {
        printf("no cassette\r\n");
        return;
    }
    printf("fwd (%s)\r\n", _playDirA ? "fwd A" : "rwd B");
    funcSequence(_playDirA, false, true);
}

void rwd()
{
    // Evacuate head, however the head direction still matters for which side the head is tracing,
    //  therefore, previous head direction is preserved and it may mean rwd A or fwd B
    if (isGearInFunc()) returnSequence();
    if (!hasCassette()) {
        printf("no cassette\r\n");
        return;
    }
    printf("rwd (%s)\r\n", _playDirA ? "rwd A" : "fwd B");
    funcSequence(_playDirA, false, false);
}

int main()
{
    stdio_init_all();

    // GPIO settings
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, 0);

    gpio_init(PIN_SOLENOID_CTRL);
    gpio_put(PIN_SOLENOID_CTRL, 1);  // set default = 1 before output mode
    gpio_set_dir(PIN_SOLENOID_CTRL, GPIO_OUT);

    gpio_init(PIN_CASSETTE_DETECT);
    gpio_set_dir(PIN_CASSETTE_DETECT, GPIO_IN);
    gpio_pull_up(PIN_CASSETTE_DETECT);

    gpio_init(PIN_FUNC_STATUS_SW);
    gpio_set_dir(PIN_FUNC_STATUS_SW, GPIO_IN);
    gpio_pull_up(PIN_FUNC_STATUS_SW);

    for (int i = 0; i < sizeof(btns_5way_tactile_plus2) / sizeof(button_t); i++) {
        button_t* button = &btns_5way_tactile_plus2[i];
        gpio_init(button->pin);
        gpio_set_dir(button->pin, GPIO_IN);
        gpio_pull_up(button->pin);
    }

    buttons = new Buttons(btns_5way_tactile_plus2, sizeof(btns_5way_tactile_plus2) / sizeof(button_t));

    // PWM settings
    gpio_pull_up(PIN_ROTATION_SENS);
    gpio_set_function(PIN_ROTATION_SENS, GPIO_FUNC_PWM);
    pwmSliceNum = pwm_gpio_to_slice_num(PIN_ROTATION_SENS);
    pwm_config pwmConfig = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&pwmConfig, PWM_DIV_B_RISING);
    pwm_config_set_clkdiv_int(&pwmConfig, 1);
    pwm_init(pwmSliceNum, &pwmConfig, true);

    // negative timeout means exact delay (rather than delay between callbacks)
    if (!add_repeating_timer_us(-INTERVAL_MS_BUTTONS_CHECK * 1000, scanButtons, nullptr, &timer)) {
        printf("Failed to add timer\n");
        return 0;
    }

    printf("CRP42602Y control started\r\n");

    stop();

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
                        if (isGearInFunc()) {
                            stop();
                        } else {
                            play(true);
                        }
                    } else if (strncmp(btnEvent.button_name, "down", 4) == 0) {
                        fwd();
                    } else if (strncmp(btnEvent.button_name, "up", 2) == 0) {
                        rwd();
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

        // Stop detection
        if (isGearInFunc() && !rotating) {
            stop();
        }
    }

    return 0;
}
