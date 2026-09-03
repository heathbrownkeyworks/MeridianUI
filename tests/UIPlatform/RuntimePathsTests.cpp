#include "Common/RuntimePaths.h"

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
    const std::filesystem::path executable = L"D:\\Games\\Skyrim Special Edition\\SkyrimSE.exe";
    const auto root = Meridian::Paths::GameRootFromExecutablePath(executable);
    Expect(root == L"D:\\Games\\Skyrim Special Edition", "game root is executable parent");
    Expect(Meridian::Paths::DataRoot(root) == root / L"Data", "data root is deterministic");
    Expect(Meridian::Paths::MeridianRoot(root) == root / L"Data" / L"MeridianUI", "Meridian root is deterministic");
    Expect(Meridian::Paths::SksePluginsRoot(root) == root / L"Data" / L"SKSE" / L"Plugins", "SKSE plugin root is deterministic");

    bool threw = false;
    try { (void)Meridian::Paths::GameRootFromExecutablePath({}); }
    catch (const std::invalid_argument&) { threw = true; }
    Expect(threw, "empty executable path is rejected");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " RuntimePaths test(s) failed\n";
        return 1;
    }
    std::cout << "All RuntimePaths tests passed\n";
    return 0;
}
