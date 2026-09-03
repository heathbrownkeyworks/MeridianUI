#include "Controllers/NifArmorModelPath.h"

#include <cstdlib>
#include <iostream>

namespace
{
    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            std::cerr << "FAILED: " << a_message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    using namespace Meridian::Controllers;

    const auto fixed = ResolveArmorModelPaths(
        "meshes/armor/hide/f/helmetlight.nif", 0);
    Expect(fixed && fixed.lowModelPath == "armor\\hide\\f\\helmetlight.nif" &&
               fixed.highModelPath.empty(),
           "a fixed ARMA model keeps one normalized path");

    const auto high = ResolveArmorModelPaths(
        "armor\\hide\\f\\cuirasslight_1.nif", ARMA_WEIGHT_SLIDER_ENABLED);
    Expect(high && high.lowModelPath == "armor\\hide\\f\\cuirasslight_0.nif" &&
               high.highModelPath == "armor\\hide\\f\\cuirasslight_1.nif",
           "a high endpoint derives its low endpoint");

    const auto low = ResolveArmorModelPaths(
        "Armor\\Hide\\F\\BootsLight_0.NIF", ARMA_WEIGHT_SLIDER_ENABLED);
    Expect(low && low.lowModelPath == "Armor\\Hide\\F\\BootsLight_0.NIF" &&
               low.highModelPath == "Armor\\Hide\\F\\BootsLight_1.NIF",
           "a low endpoint derives its high endpoint case-insensitively");

    Expect(ResolveArmorModelPaths("armor\\hide\\f\\helmetlight.nif", 2).error ==
               ArmorModelPathError::MissingWeightSuffix,
           "a weighted ARMA model requires an endpoint suffix");
    Expect(ResolveArmorModelPaths("..\\outside.nif", 0).error ==
               ArmorModelPathError::InvalidPath,
           "an unsafe ARMA path is rejected");
    Expect(ResolveArmorModelPaths("armor\\hide\\f\\boots.dds", 0).error ==
               ArmorModelPathError::InvalidPath,
           "a non-NIF ARMA path is rejected");

    Expect(IsIntentionalBlankArmorModelPath(
               "actors\\character\\character assets\\TNG\\f_blank.nif"),
           "TNG female blank body proxy is recognized");
    Expect(IsIntentionalBlankArmorModelPath(
               "Actors/Character/Character Assets/TNG/M_BLANK.NIF"),
           "blank body proxy recognition is separator and case insensitive");
    Expect(IsIntentionalBlankArmorModelPath("armor\\compat\\blank.nif"),
           "generic explicit blank proxy is recognized");
    Expect(!IsIntentionalBlankArmorModelPath("armor\\compat\\blanket.nif"),
           "ordinary model names beginning with blank remain renderable");
    Expect(!IsIntentionalBlankArmorModelPath("armor\\hide\\f\\cuirasslight_0.nif"),
           "ordinary armor geometry is not classified as a blank proxy");

    std::cout << "NIF armor model path tests passed\n";
    return EXIT_SUCCESS;
}
