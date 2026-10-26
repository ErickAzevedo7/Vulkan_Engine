// VKE_Reflect: reads the reflection marks in engine headers (reflection/Marks.h) and writes the
// C++ property tables the engine saves and edits types with.
//
//     VKE_Reflect --output <file.cpp> --function <RegisterFunction> <header>...
//
// <RegisterFunction> is a static member of Scene::ComponentRegistry; the generated file defines it.
//
// Errors are printed as "file(line): error: message" and fail the build.

#include "CodeWriter.h"
#include "HeaderParser.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
    int Usage() {
        std::cerr << "usage: VKE_Reflect --output <file.cpp> --function <name> <header>...\n";
        return 2;
    }
} // namespace

int main(int argc, char** argv) {
    std::filesystem::path output;
    std::string function;
    std::vector<std::filesystem::path> headers;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (std::strcmp(argv[i], "--function") == 0 && i + 1 < argc) {
            function = argv[++i];
        } else if (argv[i][0] == '-') {
            return Usage();
        } else {
            headers.emplace_back(argv[i]);
        }
    }
    if (output.empty() || function.empty() || headers.empty()) return Usage();

    try {
        Reflect::ReflectionModel model;
        for (const std::filesystem::path& header : headers) {
            Reflect::ParseHeader(std::filesystem::absolute(header), model);
        }
        std::string code = Reflect::WriteCode(model, function);

        std::filesystem::create_directories(output.parent_path());
        std::ofstream stream(output, std::ios::binary | std::ios::trunc);
        stream << code;
        if (!stream) {
            std::cerr << output.generic_string() << ": error: can't write the file\n";
            return 1;
        }
    } catch (const Reflect::ReflectError& error) {
        std::cerr << error.Location.File << "(" << error.Location.Line << "): error: " << error.Message << "\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "VKE_Reflect: error: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
