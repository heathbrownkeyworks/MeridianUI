#include "Input/ControllerTypes.h"
#include <cmath>
#include <iostream>
using namespace Meridian::Input;
int main()
{
    const unsigned ids[]{1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 4096, 8192, 16384, 32768, 9, 10};
    for (unsigned i = 0; i < 16; ++i)
        if (FromXInputButton(ids[i]) != static_cast<Control>(i + 1))
            return 1;
    for (unsigned i = 0; i < 16; ++i)
        if (FromSKSEKeycode(266 + i) != static_cast<Control>(i + 1))
            return 5;
    if (FromSKSEKeycode(265) != Control::None || FromSKSEKeycode(282) != Control::None || FromSKSEKeycode(1) != Control::None)
        return 6;
    if (FromXInputButton(266) != Control::None || FromXInputButton(3) != Control::None)
        return 2;
    if (ClampAxis(NAN) != 0 || ClampAxis(2) != 1 || ClampAxis(-2) != -1)
        return 3;
    if (IsKeyboardDevice(false, false) || !IsKeyboardDevice(false, true))
        return 4;
    return 0;
}
