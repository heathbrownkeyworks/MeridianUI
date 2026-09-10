#include "Common/InputDispatchFilter.h"
#include "Input/ControllerState.h"
#include <unordered_set>
#include <vector>
#include <stdexcept>
struct Node
{
    int value;
    Node* next = nullptr;
};
int main()
{
    Node d{4}, c{3, &d}, b{2, &c}, a{1, &b};
    {
        Meridian::Common::ScopedInputFilter<Node> pass(&a, [](Node*) { return false; });
        if (pass.Head() != &a || a.next != &b)
            return 1;
    }
    {
        Meridian::Common::ScopedInputFilter<Node> filtered(&a, [](Node* n) { return n->value == 2 || n->value == 4; });
        if (filtered.Head() != &a || a.next != &c || c.next)
            return 2;
        {
            Meridian::Common::ScopedInputFilter<Node> nested(filtered.Head(), [](Node* n) { return n->value == 1; });
            if (nested.Head() != &c)
                return 3;
        }
        if (a.next != &c)
            return 4;
    }
    if (a.next != &b || b.next != &c || c.next != &d || d.next)
        return 5;
    {
        Meridian::Common::ScopedInputFilter<Node> empty(&a, [](Node*) { return true; });
        if (empty.Head())
            return 6;
    }
    if (a.next != &b || b.next != &c || c.next != &d)
        return 7;
    try
    {
        Meridian::Common::ScopedInputFilter<Node> failing(&a, [](Node* n) {
            if (n->value == 3)
                throw std::runtime_error("predicate failed");
            return n->value == 2;
        });
        return 8;
    }
    catch (const std::runtime_error&)
    {
    }
    if (a.next != &b || b.next != &c || c.next != &d)
        return 9;
    // A competing direct hook and a later sink both see only the balancing
    // release and housekeeping from this mixed keyboard/controller batch.
    using namespace Meridian::Input;
    ControllerState state;
    state.Button(Control::LeftShoulder, 1);
    state.SetOwner(1);
    Node connection{4}, openingRelease{3, &connection}, gamepad{2, &openingRelease}, keyboard{1, &gamepad};
    std::unordered_set<Node*> consumed{&keyboard};
    if (state.Button(Control::South, 1).consume)
        consumed.insert(&gamepad);
    if (state.Button(Control::LeftShoulder, 0).consume)
        consumed.insert(&openingRelease);
    {
        Meridian::Common::ScopedInputFilter<Node> batch(&keyboard, [&](Node* event) { return consumed.contains(event); });
        for (unsigned sink = 0; sink < 2; ++sink)
        {
            std::vector<int> observed;
            for (auto event = batch.Head(); event; event = event->next)
                observed.push_back(event->value);
            if (observed != std::vector<int>{3, 4})
                return 10;
        }
    }
    return keyboard.next == &gamepad && gamepad.next == &openingRelease && openingRelease.next == &connection ? 0 : 11;
}
