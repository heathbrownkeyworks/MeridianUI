#include "Controllers/NifModelPath.h"

#include <cstdlib>
#include <iostream>
#include <string>

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
    std::string normalized;
    Expect(Meridian::Controllers::NormalizeNifModelPath(
               "meshes/clutter/common/coin01.NIF", normalized),
           "Data/Meshes-relative path is accepted");
    Expect(normalized == "clutter\\common\\coin01.NIF",
           "optional meshes prefix is stripped and separators are normalized");
    Expect(Meridian::Controllers::NormalizeNifModelPath("actors\\character assets\\head.nif", normalized),
           "spaces in valid asset paths are preserved");

    Expect(!Meridian::Controllers::NormalizeNifModelPath("C:\\temp\\model.nif", normalized),
           "drive-absolute path is rejected");
    Expect(!Meridian::Controllers::NormalizeNifModelPath("\\\\server\\share\\model.nif", normalized),
           "UNC path is rejected");
    Expect(!Meridian::Controllers::NormalizeNifModelPath("../outside.nif", normalized),
           "parent traversal is rejected");
    Expect(!Meridian::Controllers::NormalizeNifModelPath("clutter\\..\\outside.nif", normalized),
           "embedded parent traversal is rejected");
    Expect(!Meridian::Controllers::NormalizeNifModelPath("clutter\\\\coin01.nif", normalized),
           "empty path component is rejected");
    Expect(!Meridian::Controllers::NormalizeNifModelPath("clutter\\coin01.dds", normalized),
           "non-NIF extension is rejected");
    Expect(!Meridian::Controllers::NormalizeNifModelPath("", normalized),
           "empty path is rejected");

    std::cout << "NIF model path tests passed\n";
    return EXIT_SUCCESS;
}
