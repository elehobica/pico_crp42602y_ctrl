/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"

static constexpr uint PIN_SOLENOID_CTRL   = 2;
static constexpr uint PIN_CASSETTE_DETECT = 3;
static constexpr uint PIN_FUNC_STATUS_SW  = 4;
static constexpr uint PIN_ROTATION_SENS   = 5;  // This needs to be PWM_B pin

static uint pwmSliceNum;
static uint16_t prevRotCount = 0;
volatile static bool rotating = false;

// ADC Timer & frequency
static repeating_timer_t timer;
static constexpr int INTERVAL_MS_ROTATION_SENS_CHECK = 1000;

static bool _playDirA = true;

static bool tcRotationSensCheck(repeating_timer_t *rt) {
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
    if (!hasCassette()) return;
    printf("play A\r\n");
    funcSequence(true, true, true);
    _playDirA = true;
}

void playB()
{
    if (isGearInFunc()) returnSequence();
    if (!hasCassette()) return;
    printf("play B\r\n");
    funcSequence(false, true, false);
    _playDirA = false;
}

void fwd()
{
    // Evacuate head, however the head direction still matters for which side the head is snipping,
    //  therefore, fwdA and fwdB are the opposite reel direction.
    //  it should be re-translated if the controll is done by physical direction '<<' '>>'
    if (isGearInFunc()) returnSequence();
    if (!hasCassette()) return;
    printf("fwd %c\r\n", _playDirA ? 'A' : 'B');
    funcSequence(_playDirA, false, _playDirA);
}

void rwd()
{
    // Evacuate head, however the head direction still matters for which side the head is snipping,
    //  therefore, rwdA and rwdB are the opposite reel direction
    //  it should be re-translated if the controll is done by physical direction '<<' '>>'
    if (isGearInFunc()) returnSequence();
    if (!hasCassette()) return;
    printf("rwd %c\r\n", _playDirA ? 'A' : 'B');
    funcSequence(_playDirA, false, !_playDirA);
}

int main()
{
    stdio_init_all();

    // GPIO settings
    gpio_init(PIN_SOLENOID_CTRL);
    gpio_put(PIN_SOLENOID_CTRL, 1);  // set default = 1 before output mode
    gpio_set_dir(PIN_SOLENOID_CTRL, GPIO_OUT);

    gpio_init(PIN_CASSETTE_DETECT);
    gpio_set_dir(PIN_CASSETTE_DETECT, GPIO_IN);
    gpio_pull_up(PIN_CASSETTE_DETECT);

    gpio_init(PIN_FUNC_STATUS_SW);
    gpio_set_dir(PIN_FUNC_STATUS_SW, GPIO_IN);
    gpio_pull_up(PIN_FUNC_STATUS_SW);

    // PWM settings
    gpio_pull_up(PIN_ROTATION_SENS);
    gpio_set_function(PIN_ROTATION_SENS, GPIO_FUNC_PWM);
    pwmSliceNum = pwm_gpio_to_slice_num(PIN_ROTATION_SENS);
    pwm_config pwmConfig = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&pwmConfig, PWM_DIV_B_RISING);
    pwm_config_set_clkdiv_int(&pwmConfig, 1);
    pwm_init(pwmSliceNum, &pwmConfig, true);

    // negative timeout means exact delay (rather than delay between callbacks)
    if (!add_repeating_timer_us(-INTERVAL_MS_ROTATION_SENS_CHECK * 1000, tcRotationSensCheck, nullptr, &timer)) {
        printf("Failed to add timer\n");
        return 0;
    }

    stop();

    while (true) {
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == 's') stop();
            if (c == 'a') playA();
            if (c == 'b') playB();
            if (c == 'f') fwd();
            if (c == 'r') rwd();
        }

        // Stop detection
        if (isGearInFunc() && !rotating) {
            stop();
        }
    }

    return 0;
}
