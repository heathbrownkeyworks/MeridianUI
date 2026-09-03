#include "Controllers/LiveActorSnapshot.h"

#include <iostream>

namespace
{
    int g_failures = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }
}

int main()
{
    using namespace Meridian::Controllers::LiveActorSnapshot;
    using namespace Meridian::UI::NifScene;

    Expect(IsActorAppearanceBipedSlot(0) && IsActorAppearanceBipedSlot(31) &&
               !IsActorAppearanceBipedSlot(32) && !IsActorAppearanceBipedSlot(41),
           "actor appearance captures armor slots but excludes weapon slots");

    Expect(ShouldCaptureSource(SourceKind::Equipment,
                               ACTOR_APPEARANCE_PART_EQUIPMENT) &&
               !ShouldCaptureSource(SourceKind::Skin,
                                    ACTOR_APPEARANCE_PART_EQUIPMENT) &&
               !ShouldCaptureSource(SourceKind::Head,
                                    ACTOR_APPEARANCE_PART_EQUIPMENT),
           "equipment-only requests exclude skin and head sources");

    Expect(ShouldCaptureSource(SourceKind::Skin, ACTOR_APPEARANCE_PART_SKIN) &&
               ShouldCaptureSource(SourceKind::Head, ACTOR_APPEARANCE_PART_HEAD),
           "skin and head flags select their matching live sources");

    Expect(ClassifyBipedSource(true) == SourceKind::Skin &&
               ClassifyBipedSource(false) == SourceKind::Equipment,
           "live biped objects are classified by skin ownership");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " live actor snapshot test(s) failed\n";
        return 1;
    }
    std::cout << "All live actor snapshot tests passed\n";
    return 0;
}
