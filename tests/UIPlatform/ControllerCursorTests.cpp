#include "Input/ControllerCursorState.h"
#include <cmath>
int main()
{
    using namespace Meridian::Input;
    Meridian::Menus::CompositorMath::LayerGeometry geometry{100, 50, 1920, 1080, 1.5f};
    auto integrate = [&](int hz) {
        float x = 200, y = 200;
        for (int i = 0; i < hz; ++i)
        {
            auto next = AdvanceCursor(x, y, 900.0f / hz, 0, geometry);
            x = next.first;
            y = next.second;
        }
        return x;
    };
    if (std::abs(integrate(60) - integrate(120)) > 0.1f)
        return 1;
    auto edge = AdvanceCursor(100, 50, -100, -100, geometry);
    if (edge.first != 100 || edge.second != 50)
        return 2;
    int x = 0, y = 0;
    Meridian::Menus::CompositorMath::ScreenToBrowser(geometry, 200, 150, x, y);
    if (x != 150 || y != 150)
        return 3;
    PointerButtonState button;
    if (button.Apply(1) != 1 || button.Apply(1) != -1 || !button.Down())
        return 4;
    if (button.Apply(0) != 0 || button.Apply(0) != -1 || button.Down())
        return 5;
    return 0;
}
