#include "FontFile.h"

#include <core/Log.h>
#include <platform/FileSystem.h>

#include <cstring>

namespace VKE::UI {

    namespace {
        // TrueType stores numbers big-endian.
        u16 ReadU16(const std::vector<Byte>& data, size_t offset) {
            return static_cast<u16>((static_cast<u16>(data[offset]) << 8) | data[offset + 1]);
        }

        i16 ReadI16(const std::vector<Byte>& data, size_t offset) {
            return static_cast<i16>(ReadU16(data, offset));
        }

        u32 ReadU32(const std::vector<Byte>& data, size_t offset) {
            return (static_cast<u32>(ReadU16(data, offset)) << 16) | ReadU16(data, offset + 2);
        }

        // Offset of table `tag` in the file, or 0 when it is missing.
        size_t FindTable(const std::vector<Byte>& data, const char* tag) {
            if (data.size() < 12) return 0;
            u16 tableCount = ReadU16(data, 4);
            for (u16 i = 0; i < tableCount; ++i) {
                size_t record = 12 + static_cast<size_t>(i) * 16;
                if (record + 16 > data.size()) return 0;
                if (std::memcmp(&data[record], tag, 4) == 0) return ReadU32(data, record + 8);
            }
            return 0;
        }
    } // namespace

    FontFile FontFile::Load(const std::filesystem::path& file) {
        FontFile font;
        font.Data = Platform::FileSystem::ReadFileBytes(file.string());
        if (font.Data.empty()) return {};

        size_t head = FindTable(font.Data, "head");
        size_t hhea = FindTable(font.Data, "hhea");
        if (head == 0 || hhea == 0 || head + 20 > font.Data.size() || hhea + 8 > font.Data.size()) {
            VKE_CORE_ERROR("UI: {} is not a TrueType font the UI can use", file.string());
            return {};
        }
        font.UnitsPerEm = static_cast<f32>(ReadU16(font.Data, head + 18));
        font.Ascender = static_cast<f32>(ReadI16(font.Data, hhea + 4));
        font.Descender = static_cast<f32>(ReadI16(font.Data, hhea + 6));
        if (font.UnitsPerEm <= 0.0f || font.Ascender - font.Descender <= 0.0f) {
            VKE_CORE_ERROR("UI: {} has no usable vertical metrics", file.string());
            return {};
        }
        return font;
    }

} // namespace VKE::UI
