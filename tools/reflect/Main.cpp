// VKE_Reflect: reads the reflection marks in headers (reflection/Marks.h, vke/Marks.h) and writes
// the C++ the engine and games are built with.
//
// The engine's property tables. <RegisterFunction> is a static member of Scene::ComponentRegistry,
// which the generated file defines:
//     VKE_Reflect --output <file.cpp> --function <RegisterFunction> <header>...
//
// A game's script tables, from the headers in its scripts folder, which can be none:
//     VKE_Reflect --game --root <scripts folder> --output <file.cpp> [<header>...]
//
// The table between the SDK and the engine, as files in <folder>:
//     VKE_Reflect --api --output <folder> <SDK header>...
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
        std::cerr << "usage: VKE_Reflect --output <file.cpp> --function <name> <header>...\n"
                     "       VKE_Reflect --game --root <folder> --output <file.cpp> [<header>...]\n"
                     "       VKE_Reflect --api --output <folder> <header>...\n";
        return 2;
    }

    void WriteFile(const std::filesystem::path& file, const std::string& text) {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream << text;
        if (!stream) throw Reflect::ReflectError{{file.generic_string(), 0}, "can't write the file"};
    }
} // namespace

int main(int argc, char** argv) {
    std::filesystem::path output;
    std::filesystem::path root;
    std::string function;
    bool game = false;
    bool api = false;
    std::vector<std::filesystem::path> headers;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (std::strcmp(argv[i], "--function") == 0 && i + 1 < argc) {
            function = argv[++i];
        } else if (std::strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = std::filesystem::absolute(argv[++i]);
        } else if (std::strcmp(argv[i], "--game") == 0) {
            game = true;
        } else if (std::strcmp(argv[i], "--api") == 0) {
            api = true;
        } else if (argv[i][0] == '-') {
            return Usage();
        } else {
            headers.emplace_back(argv[i]);
        }
    }
    if (output.empty() || (game && api)) return Usage();
    if (game ? root.empty() : headers.empty()) return Usage();
    if (!game && !api && function.empty()) return Usage();

    try {
        Reflect::ReflectionModel model;
        for (const std::filesystem::path& header : headers) {
            Reflect::ParseHeader(std::filesystem::absolute(header), root, model);
        }

        if (api) {
            for (const auto& [name, text] : Reflect::WriteApiFiles(model)) {
                WriteFile(output / name, text);
            }
        } else {
            WriteFile(output, game ? Reflect::WriteGameCode(model) : Reflect::WriteCode(model, function));
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
