#include "Controllers/ActorAppearancePolicy.h"

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
    using namespace Meridian::Controllers::ActorAppearance;

    Expect(ShouldIncludeSkinAddon(BIPED_BODY, 0),
           "uncovered body skin is included");
    Expect(!ShouldIncludeSkinAddon(BIPED_BODY, BIPED_BODY),
           "covered body skin is excluded");
    Expect(!ShouldIncludeSkinAddon(BIPED_BODY | BIPED_HANDS, BIPED_HANDS),
           "a partially covered multi-slot skin add-on is excluded conservatively");
    Expect(!ShouldIncludeSkinAddon(0, 0),
           "slotless skin add-ons are excluded");

    Expect(ShouldIncludeHeadPart(HeadPartKind::Hair, 0),
           "uncovered hair is included");
    Expect(!ShouldIncludeHeadPart(HeadPartKind::Hair, BIPED_HEAD),
           "head coverage excludes hair");
    Expect(!ShouldIncludeHeadPart(HeadPartKind::Hair, BIPED_HAIR),
           "hair coverage excludes hair");
    Expect(!ShouldIncludeHeadPart(HeadPartKind::Hair, BIPED_LONG_HAIR),
           "long-hair coverage excludes hair");
    Expect(ShouldIncludeHeadPart(HeadPartKind::Hair, BIPED_CIRCLET),
           "circlets do not automatically exclude hair");

    Expect(ShouldIncludeHeadPart(HeadPartKind::Face, 0) &&
               ShouldIncludeHeadPart(HeadPartKind::Eyes, BIPED_HAIR),
           "uncovered facial foundations are included");
    Expect(!ShouldIncludeHeadPart(HeadPartKind::Face, BIPED_HEAD) &&
               !ShouldIncludeHeadPart(HeadPartKind::Eyes, BIPED_DECAPITATE_HEAD),
           "head and decapitation coverage exclude facial foundations");

    return g_failures == 0 ? 0 : 1;
}
