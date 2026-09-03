#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

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

    std::string ReadSource(const std::filesystem::path& a_relativePath)
    {
        const auto path = std::filesystem::path(MERIDIAN_SOURCE_DIR) / a_relativePath;
        std::ifstream stream(path, std::ios::binary);
        std::ostringstream contents;
        contents << stream.rdbuf();
        Expect(stream.good() || stream.eof(), "web-contract source file is readable");
        return contents.str();
    }

    bool Contains(std::string_view a_text, std::string_view a_needle)
    {
        return a_text.find(a_needle) != std::string_view::npos;
    }
}

int main()
{
    const auto html = ReadSource("src/NifTest/web/index.html");
    const auto script = ReadSource("src/NifTest/web/camera-controls.js");
    const auto consumer = ReadSource("src/NifTest/main.cpp");

    Expect(Contains(html, "data-action=\"orbitLeft\"") &&
               Contains(html, "data-action=\"orbitRight\""),
           "both press-and-hold orbit controls exist");
    Expect(Contains(script, "requestAnimationFrame"),
           "camera motion is synchronized to browser frames");
    Expect(!Contains(script, "setInterval"),
           "camera motion does not use timer-based repetition");
    Expect(Contains(script, "setPointerCapture") &&
               Contains(script, "pointercancel") &&
               Contains(script, "lostpointercapture"),
           "pointer holds capture and cancel safely");
    Expect(Contains(script, "visibilitychange") &&
               Contains(script, "pagehide") &&
               Contains(script, "blur"),
           "all page-lifecycle paths cancel held controls");
    Expect(Contains(script, "event.shiftKey") && Contains(script, "panLeft"),
           "keyboard hold supports shifted pan controls");
    Expect(Contains(script, "meridianNifSetCamera") &&
               Contains(consumer, "meridianNifSetCamera"),
           "camera listener name matches across the bridge");
    Expect(Contains(script, "meridianNifLayout") &&
               Contains(consumer, "meridianNifLayout"),
           "responsive layout listener name matches across the bridge");
    Expect(Contains(script, "typeof window.meridianNifLayout !== \"function\"") &&
               Contains(script, "scheduleLayoutReport();"),
           "initial layout reporting retries until the Meridian listener is injected");
    Expect(Contains(consumer, "applied preview layout") &&
               Contains(consumer, "rejected preview layout") &&
               Contains(consumer, "surfaceInfo.initiallyVisible = false") &&
               Contains(consumer, "info.initiallyVisible = false") &&
               Contains(consumer, "SetVisible(state.surface, state.testVisible)"),
           "both control and native layers stay hidden until the test is explicitly opened");
    Expect(Contains(consumer, "NifTestHotkeySink") &&
               Contains(consumer, "IsToggleHotkey") &&
               Contains(consumer, "SetTestVisible(!State().testVisible)") &&
               Contains(consumer, "press Alt+N to open"),
           "Alt+N owns an explicit hidden-view toggle path");
    Expect(Contains(html, "data-model-path=\"armor\\hide\\f\\cuirasslight_1.nif\"") &&
               Contains(html, "data-model-path=\"clutter\\coin01.nif\"") &&
               Contains(html, "id=\"modelPath\""),
           "skinned, static, and arbitrary model selectors exist");
    Expect(Contains(script, "meridianNifLoad") &&
               Contains(consumer, "meridianNifLoad"),
           "model-load listener name matches across the bridge");
    Expect(Contains(html, "data-armor-outfit-preset") &&
               Contains(html, "data-modded-armor-outfit-preset") &&
               Contains(html, "data-layered-armor-preset") &&
               Contains(html, "data-texture-swap-preset") &&
               Contains(html, "data-player-outfit-preset") &&
               Contains(html, "data-player-actor-preset") &&
               Contains(html, "data-outfit-preset") &&
               Contains(html, "data-outfit-object=\"101\"") &&
               Contains(html, "data-outfit-object=\"102\"") &&
               Contains(html, "data-outfit-object=\"103\"") &&
               Contains(html, "data-outfit-object=\"104\""),
           "vanilla, modded, layered, texture-swap, player-equipped, player-actor, and NIF hide-outfit presets expose stable handles");
    Expect(Contains(script, "meridianNifLoadOutfit") &&
               Contains(consumer, "meridianNifLoadOutfit") &&
               Contains(script, "meridianNifLoadArmorOutfit") &&
               Contains(consumer, "meridianNifLoadArmorOutfit") &&
               Contains(script, "meridianNifLoadModdedArmorOutfit") &&
               Contains(consumer, "meridianNifLoadModdedArmorOutfit") &&
               Contains(script, "meridianNifLoadLayeredArmor") &&
               Contains(consumer, "meridianNifLoadLayeredArmor") &&
               Contains(script, "meridianNifLoadTextureSwapArmor") &&
               Contains(consumer, "meridianNifLoadTextureSwapArmor") &&
               Contains(script, "meridianNifLoadPlayerOutfit") &&
               Contains(consumer, "meridianNifLoadPlayerOutfit") &&
               Contains(script, "meridianNifLoadPlayerActor") &&
               Contains(consumer, "meridianNifLoadPlayerActor") &&
               Contains(script, "meridianNifSetObjectVisible") &&
               Contains(consumer, "meridianNifSetObjectVisible"),
           "vanilla, modded, layered, texture-swap, player-equipped, player-actor, NIF outfit, and visibility listeners match across the bridge");
    Expect(Contains(consumer, "Query4") &&
               Contains(consumer, "ActorAppearanceSceneLoadInfo") &&
               Contains(consumer, "actorFormID = 0x14") &&
               Contains(consumer, "ACTOR_APPEARANCE_PARTS_ALL") &&
               Contains(consumer, "ReplaceActorAppearanceScene") &&
               Contains(script, "PLAYER-ACTOR") &&
               Contains(script, "LIVE ACTOR FOUNDATION"),
           "the standalone actor preset requests the complete NifScene/4 foundation without reading actor state on the CEF callback");
    Expect(Contains(consumer, "SKSE::GetTaskInterface()") &&
               Contains(consumer, "PlayerCharacter::GetSingleton()") &&
               Contains(consumer, "GetWornArmor") &&
               Contains(consumer, "BuildEquippedArmorManifest") &&
               Contains(consumer, "GetActorBase()") &&
               Contains(consumer, "base->GetSex()") &&
               Contains(consumer, "base->weight") &&
               Contains(consumer, "ReplaceArmorScene"),
           "player equipment is discovered on Skyrim's task and reuses the bounded armor scene pipeline");
    Expect(Contains(script, "RUNTIME EQUIPPED") &&
               Contains(script, "meridianNifApplyActorState") &&
               Contains(consumer, "meridianNifApplyActorState") &&
               Contains(script, "meridianNifSyncWeight") &&
               Contains(consumer, "meridianNifSyncWeight"),
           "dynamic actor mode disables fixed slots and synchronizes the resolved actor weight");
    Expect(Contains(html, "data-outfit-weight=\"0\"") &&
               Contains(html, "data-outfit-weight=\"50\"") &&
               Contains(html, "data-outfit-weight=\"100\"") &&
               Contains(html, "id=\"weightReadout\""),
           "the outfit exposes a three-point body-weight calibration rail");
    Expect(Contains(script, "meridianNifSetWeight") &&
               Contains(consumer, "meridianNifSetWeight") &&
               Contains(consumer, "ParseWeightPayload") &&
               Contains(consumer, "Query4") &&
               Contains(consumer, "ReplaceArmorScene"),
           "weight listener and the inherited NifScene armor path match across the version 4 bridge");
    Expect(!Contains(consumer, "LookupByEditorID<RE::TESObjectARMO>") &&
               Contains(consumer, "0x00013746") &&
               Contains(consumer, "0x00013911") &&
               Contains(consumer, "0x00013913") &&
               Contains(consumer, "0x00013912") &&
               Contains(consumer, "0x00013910"),
           "the vanilla record fixture uses deterministic Skyrim.esm FormIDs");
    Expect(Contains(consumer, "LookupForm<RE::TESObjectARMO>") &&
               Contains(consumer, "BD Standalone.esp") &&
               Contains(consumer, "0x000B28") &&
               Contains(consumer, "0x000AF3") &&
               Contains(consumer, "0x000B51"),
           "the modded record fixture uses deterministic BD Standalone plugin-local FormIDs");
    Expect(Contains(consumer, "MeridianNifTest.esp") &&
               Contains(consumer, "0x000800") &&
               Contains(consumer, "LAYERED_OBJECT"),
           "the layered fixture uses one deterministic plugin-local ARMO and public handle");
    Expect(Contains(consumer, "0x000803") &&
               Contains(consumer, "TEXTURE_SWAP_OBJECT"),
           "the alternate-texture fixture uses one deterministic plugin-local ARMO and public handle");
    Expect(Contains(html, "id=\"outfitReadout\"") &&
               Contains(script, "updateOutfitReadout") &&
               Contains(script, "aria-pressed"),
           "the outfit manifest reports accessible per-slot visibility state");
    Expect(Contains(html, "data-lighting-preset=\"0\"") &&
               Contains(html, "data-lighting-preset=\"1\"") &&
               Contains(html, "data-lighting-preset=\"2\"") &&
               Contains(html, "id=\"exposure\"") &&
               Contains(html, "id=\"resetLighting\""),
           "neutral, bright, dramatic, exposure, and reset controls exist");
    Expect(Contains(script, "meridianNifSetLighting") &&
               Contains(consumer, "meridianNifSetLighting") &&
               Contains(consumer, "SetLightingFromPage"),
           "lighting listener name matches across the bridge");
    Expect(Contains(script, "aria-pressed") && Contains(script, "is-selected"),
           "lighting preset selection has visible and accessible state");
    Expect(Contains(script, "event.target instanceof HTMLInputElement"),
           "camera hotkeys do not fire while editing a model path");
    Expect(Contains(script, "event.key === \"Escape\"") &&
               Contains(script, "stopAllActions()") &&
               Contains(script, "meridianNifClose") &&
               Contains(consumer, "meridianNifClose"),
           "Escape cancels holds and closes both control layers");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " NIF control web contract test(s) failed\n";
        return 1;
    }

    std::cout << "All NIF control web contract tests passed\n";
    return 0;
}
