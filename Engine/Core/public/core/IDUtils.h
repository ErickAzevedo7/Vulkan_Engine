#pragma once

#include <core/Types.h>

#include <string>

namespace VKE {

    // Helpers shared by the 64-bit ID types that are saved in files (ObjectID, Assets::AssetID).

    // A new random ID, never zero, drawn from the operating system's random source so separate
    // runs don't repeat each other's IDs.
    u64 GenerateID();

    // How IDs are written in files: sixteen hex digits, zero-padded.
    std::string FormatID(u64 id);

    // The value of an ID written by FormatID, either case. Zero, which no valid ID has, for
    // anything that isn't exactly sixteen hex digits.
    u64 ParseID(const std::string& text);

} // namespace VKE
