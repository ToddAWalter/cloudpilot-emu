//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _KEYS_H_
#define _KEYS_H_

#include <stdbool.h>
#include <stdint.h>

#include "soc_GPIO.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Keypad;

enum KeyId {
    keyInvalid = 0,
    keyIdHard1 = 1,
    keyIdHard2 = 2,
    keyIdHard3 = 3,
    keyIdHard4 = 4,
    keyIdUp = 5,
    keyIdDown = 6,
    keyIdLeft = 7,
    keyIdRight = 8,
    keyIdSelect = 9,
    keyIdPower = 10
};

struct Keypad *keypadInit(struct SocGpio *gpio, bool matrixHasPullUps);
bool keypadDefineRow(struct Keypad *kp, unsigned rowIdx, int8_t gpio);
bool keypadDefineCol(struct Keypad *kp, unsigned colIdx, int8_t gpio);
bool keypadAddGpioKey(struct Keypad *kp, enum KeyId key, int8_t gpioNum, bool activeHigh);
bool keypadAddMatrixKey(struct Keypad *kp, enum KeyId key, unsigned row, unsigned col);

void keypadKeyEvt(struct Keypad *kp, enum KeyId key, bool wentDown);

void keypadReset(struct Keypad *kp);

#ifdef __cplusplus
}
#endif

#endif
