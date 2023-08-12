/*------------------------------------------------------/
/ Copyright (c) 2023, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <stdio.h>
#include "pico/stdlib.h"

static constexpr uint PIN_SOLENOID_CTRL = 19;
static bool _playDirA = true;

void ctrlSolenoid(bool flag)
{
    if (flag) {
        gpio_put(PIN_SOLENOID_CTRL, 0);
    } else {
        gpio_put(PIN_SOLENOID_CTRL, 1);
    }
}

void funcSequence(bool pinchDirA, bool headPosPlay, bool reelFwd)
{
    // Function sequence has 190 degree of function gear to roll in 0.4 second
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
    ctrlSolenoid(true);
    sleep_ms(20);
    ctrlSolenoid(false);
}

void stop()
{
    printf("stop\r\n");
    returnSequence();
}

void playA()
{
    printf("play A\r\n");
    funcSequence(true, true, true);
    _playDirA = true;
}

void playB()
{
    printf("play B\r\n");
    funcSequence(false, true, false);
    _playDirA = false;
}

void fwd()
{
    // Evacuate head, however the head direction still matters for which side the head is snipping,
    //  therefore, fwdA and fwdB are the opposite reel direction.
    //  it should be re-translated if the controll is done by physical direction '<<' '>>'
    printf("fwd%c\r\n", _playDirA ? 'A' : 'B');
    funcSequence(_playDirA, false, _playDirA);
}

void rwd()
{
    // Evacuate head, however the head direction still matters for which side the head is snipping,
    //  therefore, rwdA and rwdB are the opposite reel direction
    //  it should be re-translated if the controll is done by physical direction '<<' '>>'
    printf("rwd%c\r\n", _playDirA ? 'A' : 'B');
    funcSequence(_playDirA, false, !_playDirA);
}

int main()
{
    stdio_init_all();

    gpio_init(PIN_SOLENOID_CTRL);
    gpio_put(PIN_SOLENOID_CTRL, 1);  // set default = 1 before output mode
    gpio_set_dir(PIN_SOLENOID_CTRL, GPIO_OUT);

    while (true) {
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == 's') stop();
            if (c == 'a') playA();
            if (c == 'b') playB();
            if (c == 'f') fwd();
            if (c == 'r') rwd();
        }
    }

    return 0;
}
