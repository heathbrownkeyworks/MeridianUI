# Unit/integration test targets, registered with CTest when BUILD_TESTING is ON.
# Included from the top-level CMakeLists.txt. Requires the caller to have
# already included CTest and defined the `set_compile_options()` function.

if(BUILD_TESTING)
    add_test(
        NAME BuildReleaseScriptTests
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/BuildReleaseScriptTests.ps1"
                -SourceRoot "${CMAKE_CURRENT_SOURCE_DIR}"
                -ScratchRoot "${CMAKE_CURRENT_BINARY_DIR}/BuildReleaseScriptTests"
    )
    add_executable(BuildConfigurationTests tests/UIPlatform/BuildConfigurationTests.cpp)
    set_compile_options(BuildConfigurationTests)
    target_compile_definitions(BuildConfigurationTests PRIVATE
        $<$<CONFIG:Release>:MERIDIAN_EXPECT_RELEASE>
        $<$<CONFIG:Debug>:MERIDIAN_EXPECT_DEBUG>)
    add_test(NAME BuildConfigurationTests COMMAND BuildConfigurationTests)

    # Exercise the production service with a counted CEF boundary. No Skyrim
    # process or libcef.dll is loaded by these failure-injection tests.
    add_executable(
        CEFServiceStartupTests
            tests/UIPlatform/CEFServiceStartupTests.cpp
            src/UIPlatform/Services/CEFService.cpp
    )
    target_include_directories(
        CEFServiceStartupTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/UIPlatform/CEFServiceStubs
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CEFServiceStartupTests)
    add_test(NAME CEFStartupCode38Tests COMMAND CEFServiceStartupTests code38)
    add_test(NAME CEFStartupOtherFailureTests COMMAND CEFServiceStartupTests code1)
    add_test(NAME CEFStartupSuccessTests COMMAND CEFServiceStartupTests success)

    add_executable(
        ProcessDownDetectorTests
            tests/CEFSubprocess/ProcessDownDetectorTests.cpp
            src/CEFSubprocess/ProcessDownDetector.cpp
    )
    target_include_directories(
        ProcessDownDetectorTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/CEFSubprocess
    )
    set_compile_options(ProcessDownDetectorTests)
    add_test(NAME ProcessDownDetectorTests COMMAND ProcessDownDetectorTests)

    add_executable(
        PromiseRegistryTests
            tests/CEFSubprocess/PromiseRegistryTests.cpp
    )
    target_include_directories(
        PromiseRegistryTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/CEFSubprocess
    )
    set_compile_options(PromiseRegistryTests)
    add_test(NAME PromiseRegistryTests COMMAND PromiseRegistryTests)

    add_executable(
        JsBindingMessageTests
            tests/IPC/JsBindingMessageTests.cpp
    )
    target_include_directories(
        JsBindingMessageTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/CEFSubprocess
    )
    set_compile_options(JsBindingMessageTests)
    add_test(NAME JsBindingMessageTests COMMAND JsBindingMessageTests)

    add_executable(
        VersionGateTests
            tests/UIPlatform/VersionGateTests.cpp
    )
    target_include_directories(
        VersionGateTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(VersionGateTests)
    add_test(NAME VersionGateTests COMMAND VersionGateTests)

    add_executable(
        ViewAPIContractTests
            tests/UIPlatform/ViewAPIContractTests.cpp
    )
    target_include_directories(
        ViewAPIContractTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(ViewAPIContractTests)
    add_test(NAME ViewAPIContractTests COMMAND ViewAPIContractTests)

    add_executable(
        RenderLayerAPIContractTests
            tests/UIPlatform/RenderLayerAPIContractTests.cpp
    )
    target_include_directories(
        RenderLayerAPIContractTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(RenderLayerAPIContractTests)
    add_test(NAME RenderLayerAPIContractTests COMMAND RenderLayerAPIContractTests)

    add_executable(
        NifViewAPIContractTests
            tests/UIPlatform/NifViewAPIContractTests.cpp
    )
    target_include_directories(
        NifViewAPIContractTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifViewAPIContractTests)
    add_test(NAME NifViewAPIContractTests COMMAND NifViewAPIContractTests)

    add_executable(
        NifSceneAPIContractTests
            tests/UIPlatform/NifSceneAPIContractTests.cpp
    )
    target_include_directories(
        NifSceneAPIContractTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifSceneAPIContractTests)
    add_test(NAME NifSceneAPIContractTests COMMAND NifSceneAPIContractTests)

    add_executable(
        CheckerboardTextureTests
            tests/UIPlatform/CheckerboardTextureTests.cpp
    )
    target_include_directories(
        CheckerboardTextureTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CheckerboardTextureTests)
    add_test(NAME CheckerboardTextureTests COMMAND CheckerboardTextureTests)

    add_executable(
        PackedGeometryDecoderTests
            tests/UIPlatform/PackedGeometryDecoderTests.cpp
    )
    target_include_directories(
        PackedGeometryDecoderTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(PackedGeometryDecoderTests)
    add_test(NAME PackedGeometryDecoderTests COMMAND PackedGeometryDecoderTests)

    add_executable(
        BindPoseSkinningTests
            tests/UIPlatform/BindPoseSkinningTests.cpp
    )
    target_include_directories(
        BindPoseSkinningTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(BindPoseSkinningTests)
    add_test(NAME BindPoseSkinningTests COMMAND BindPoseSkinningTests)

    add_executable(
        NifMaterialTintTests
            tests/UIPlatform/NifMaterialTintTests.cpp
    )
    target_include_directories(
        NifMaterialTintTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifMaterialTintTests)
    add_test(NAME NifMaterialTintTests COMMAND NifMaterialTintTests)

    add_executable(
        FaceGenMaterialTests
            tests/UIPlatform/FaceGenMaterialTests.cpp
    )
    target_include_directories(
        FaceGenMaterialTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(FaceGenMaterialTests)
    add_test(NAME FaceGenMaterialTests COMMAND FaceGenMaterialTests)

    add_executable(
        NifSceneCompositionTests
            tests/UIPlatform/NifSceneCompositionTests.cpp
    )
    target_include_directories(
        NifSceneCompositionTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifSceneCompositionTests)
    add_test(NAME NifSceneCompositionTests COMMAND NifSceneCompositionTests)

    add_executable(
        NifWeightInterpolationTests
            tests/UIPlatform/NifWeightInterpolationTests.cpp
    )
    target_include_directories(
        NifWeightInterpolationTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifWeightInterpolationTests)
    add_test(NAME NifWeightInterpolationTests COMMAND NifWeightInterpolationTests)

    add_executable(
        NifModelPathTests
            tests/UIPlatform/NifModelPathTests.cpp
    )
    target_include_directories(
        NifModelPathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifModelPathTests)
    add_test(NAME NifModelPathTests COMMAND NifModelPathTests)

    add_executable(
        NifArmorModelPathTests
            tests/UIPlatform/NifArmorModelPathTests.cpp
    )
    target_include_directories(
        NifArmorModelPathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifArmorModelPathTests)
    add_test(NAME NifArmorModelPathTests COMMAND NifArmorModelPathTests)

    add_executable(
        ActorAppearancePolicyTests
            tests/UIPlatform/ActorAppearancePolicyTests.cpp
    )
    target_include_directories(
        ActorAppearancePolicyTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(ActorAppearancePolicyTests)
    add_test(NAME ActorAppearancePolicyTests COMMAND ActorAppearancePolicyTests)

    add_executable(
        LiveActorSnapshotTests
            tests/UIPlatform/LiveActorSnapshotTests.cpp
    )
    target_include_directories(
        LiveActorSnapshotTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(LiveActorSnapshotTests)
    add_test(NAME LiveActorSnapshotTests COMMAND LiveActorSnapshotTests)

    add_executable(
        NifTexturePathTests
            tests/UIPlatform/NifTexturePathTests.cpp
    )
    target_include_directories(
        NifTexturePathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifTexturePathTests)
    add_test(NAME NifTexturePathTests COMMAND NifTexturePathTests)

    add_executable(
        NifTextureOverrideTests
            tests/UIPlatform/NifTextureOverrideTests.cpp
    )
    target_include_directories(
        NifTextureOverrideTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifTextureOverrideTests)
    add_test(NAME NifTextureOverrideTests COMMAND NifTextureOverrideTests)

    add_executable(
        EquippedArmorManifestTests
            tests/UIPlatform/EquippedArmorManifestTests.cpp
    )
    target_include_directories(
        EquippedArmorManifestTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest
    )
    set_compile_options(EquippedArmorManifestTests)
    add_test(NAME EquippedArmorManifestTests COMMAND EquippedArmorManifestTests)

    add_executable(
        NifTestHotkeyTests
            tests/UIPlatform/NifTestHotkeyTests.cpp
    )
    target_include_directories(
        NifTestHotkeyTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest
    )
    set_compile_options(NifTestHotkeyTests)
    add_test(NAME NifTestHotkeyTests COMMAND NifTestHotkeyTests)

    add_executable(
        NifTestControlPayloadTests
            tests/UIPlatform/NifTestControlPayloadTests.cpp
    )
    target_include_directories(
        NifTestControlPayloadTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifTestControlPayloadTests)
    add_test(NAME NifTestControlPayloadTests COMMAND NifTestControlPayloadTests)

    add_executable(
        NifCameraMathTests
            tests/UIPlatform/NifCameraMathTests.cpp
    )
    target_include_directories(
        NifCameraMathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifCameraMathTests)
    add_test(NAME NifCameraMathTests COMMAND NifCameraMathTests)

    add_executable(
        NifLightingTests
            tests/UIPlatform/NifLightingTests.cpp
    )
    target_include_directories(
        NifLightingTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifLightingTests)
    add_test(NAME NifLightingTests COMMAND NifLightingTests)

    add_executable(
        NifControlWebContractTests
            tests/UIPlatform/NifControlWebContractTests.cpp
    )
    target_compile_definitions(
        NifControlWebContractTests
        PRIVATE
            MERIDIAN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    set_compile_options(NifControlWebContractTests)
    add_test(NAME NifControlWebContractTests COMMAND NifControlWebContractTests)

    add_executable(
        NifPreviewRendererArchitectureTests
            tests/UIPlatform/NifPreviewRendererArchitectureTests.cpp
    )
    target_compile_definitions(
        NifPreviewRendererArchitectureTests
        PRIVATE
            MERIDIAN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    set_compile_options(NifPreviewRendererArchitectureTests)
    add_test(NAME NifPreviewRendererArchitectureTests COMMAND NifPreviewRendererArchitectureTests)

    add_executable(
        ViewBridgeScriptTests
            tests/UIPlatform/ViewBridgeScriptTests.cpp
            src/UIPlatform/Controllers/ViewBridgeScripts.cpp
    )
    target_include_directories(
        ViewBridgeScriptTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    target_link_libraries(
        ViewBridgeScriptTests
        PRIVATE
            nlohmann_json::nlohmann_json
    )
    set_compile_options(ViewBridgeScriptTests)
    add_test(NAME ViewBridgeScriptTests COMMAND ViewBridgeScriptTests)

    add_executable(
        CompositorMathTests
            tests/UIPlatform/CompositorMathTests.cpp
    )
    target_include_directories(
        CompositorMathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CompositorMathTests)
    add_test(NAME CompositorMathTests COMMAND CompositorMathTests)

    add_executable(
        SettingsIngestTests
            tests/UIPlatform/SettingsIngestTests.cpp
    )
    target_include_directories(
        SettingsIngestTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(SettingsIngestTests)
    add_test(NAME SettingsIngestTests COMMAND SettingsIngestTests)

    add_executable(
        IniConfigTests
            tests/UIPlatform/IniConfigTests.cpp
            src/UIPlatform/Config/IniConfig.cpp
    )
    target_include_directories(
        IniConfigTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    # IniConfig.cpp includes <SimpleIni.h> directly; link the header-only
    # SimpleIni interface target so its include dir is visible (the main target
    # picks this up transitively from its other vcpkg dependencies).
    target_link_libraries(IniConfigTests PRIVATE SimpleIni::SimpleIni)
    target_compile_definitions(
        IniConfigTests
        PRIVATE
            MERIDIAN_INICONFIG_NO_RUNTIME=1
    )
    set_compile_options(IniConfigTests)
    add_test(NAME IniConfigTests COMMAND IniConfigTests)

    add_executable(
        CursorTextureCacheTests
            tests/UIPlatform/CursorTextureCacheTests.cpp
    )
    target_include_directories(
        CursorTextureCacheTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CursorTextureCacheTests)
    add_test(NAME CursorTextureCacheTests COMMAND CursorTextureCacheTests)

    add_executable(
        FrameTransportTests
            tests/UIPlatform/FrameTransportTests.cpp
            src/UIPlatform/Render/RenderDevice.cpp
            src/UIPlatform/Render/FrameTransport.cpp
    )
    target_include_directories(
        FrameTransportTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    target_compile_definitions(
        FrameTransportTests
        PRIVATE
            MERIDIAN_FRAME_TRANSPORT_NO_PCH=1
    )
    target_link_libraries(
        FrameTransportTests
        PRIVATE
            d3d11.lib
            dxgi.lib
    )
    set_compile_options(FrameTransportTests)
    add_test(NAME FrameTransportTests COMMAND FrameTransportTests)

    add_executable(
        InputChordTests
            tests/UIPlatform/InputChordTests.cpp
    )
    target_include_directories(
        InputChordTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(InputChordTests)
    add_test(NAME InputChordTests COMMAND InputChordTests)

    add_executable(
        InputSinkPriorityTests
            tests/UIPlatform/InputSinkPriorityTests.cpp
    )
    target_include_directories(
        InputSinkPriorityTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(InputSinkPriorityTests)
    add_test(NAME InputSinkPriorityTests COMMAND InputSinkPriorityTests)

    add_executable(
        PressedKeyStateTests
            tests/UIPlatform/PressedKeyStateTests.cpp
    )
    target_include_directories(
        PressedKeyStateTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(PressedKeyStateTests)
    add_test(NAME PressedKeyStateTests COMMAND PressedKeyStateTests)

    add_executable(
        CallSiteValidationTests
            tests/UIPlatform/CallSiteValidationTests.cpp
    )
    target_include_directories(
        CallSiteValidationTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CallSiteValidationTests)
    add_test(NAME CallSiteValidationTests COMMAND CallSiteValidationTests)

    add_executable(
        ModSchemePathTests
            tests/UIPlatform/ModSchemePathTests.cpp
            src/UIPlatform/Scheme/ModSchemePath.cpp
    )
    target_include_directories(
        ModSchemePathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(ModSchemePathTests)
    add_test(NAME ModSchemePathTests COMMAND ModSchemePathTests)

    add_executable(
        WeakFocusOwnerTests
            tests/UIPlatform/WeakFocusOwnerTests.cpp
    )
    target_include_directories(
        WeakFocusOwnerTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(WeakFocusOwnerTests)
    add_test(NAME WeakFocusOwnerTests COMMAND WeakFocusOwnerTests)

    add_executable(
        TextInputLeaseStateTests
            tests/UIPlatform/TextInputLeaseStateTests.cpp
    )
    target_include_directories(
        TextInputLeaseStateTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(TextInputLeaseStateTests)
    add_test(NAME TextInputLeaseStateTests COMMAND TextInputLeaseStateTests)

    add_executable(
        BrowserContentPolicyTests
            tests/UIPlatform/BrowserContentPolicyTests.cpp
            src/UIPlatform/CEF/BrowserContentPolicy.cpp
            src/UIPlatform/Scheme/ModSchemePath.cpp
    )
    target_include_directories(
        BrowserContentPolicyTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(BrowserContentPolicyTests)
    add_test(NAME BrowserContentPolicyTests COMMAND BrowserContentPolicyTests)

    add_executable(
        PublicAPIValidationTests
            tests/UIPlatform/PublicAPIValidationTests.cpp
    )
    target_include_directories(
        PublicAPIValidationTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(PublicAPIValidationTests)
    add_test(NAME PublicAPIValidationTests COMMAND PublicAPIValidationTests)

    add_executable(
        RuntimePathsTests
            tests/UIPlatform/RuntimePathsTests.cpp
    )
    target_include_directories(
        RuntimePathsTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_compile_options(RuntimePathsTests)
    add_test(NAME RuntimePathsTests COMMAND RuntimePathsTests)

    add_executable(
        CursorPolicyTests
            tests/UIPlatform/CursorPolicyTests.cpp
    )
    target_include_directories(
        CursorPolicyTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CursorPolicyTests)
    add_test(NAME CursorPolicyTests COMMAND CursorPolicyTests)

    add_executable(
        RendererSelectionTests
            tests/UIPlatform/RendererSelectionTests.cpp
    )
    target_include_directories(
        RendererSelectionTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(RendererSelectionTests)
    add_test(NAME RendererSelectionTests COMMAND RendererSelectionTests)

    add_executable(
        FocusSessionStateTests
            tests/UIPlatform/FocusSessionStateTests.cpp
    )
    target_include_directories(
        FocusSessionStateTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(FocusSessionStateTests)
    add_test(NAME FocusSessionStateTests COMMAND FocusSessionStateTests)
endif()
