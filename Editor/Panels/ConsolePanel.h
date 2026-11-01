#pragma once

#include "LogCapture.h"

#include <core/Types.h>

#include <array>
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

namespace VKE::Editor {

    // The engine's log, newest at the bottom. The same message logged several times in a row
    // shows once, with a count. Messages can be filtered by level and text, and copied. Those of
    // the compiler point at a file and line, and open there with a double click.
    class ConsolePanel {
    public:
        // The category of the compiler's messages, which start with "file(line): ".
        static constexpr const char* kCompilerCategory = "Compiler";

        // The window's title, with its icon.
        static const char* GetWindowName();

        // Takes what was logged since the last frame. Call every frame, shown or not.
        void Update();
        void Draw(bool* open);

        // The newest message, or null when there is none.
        const LogEntry* GetLast() const;

        // What the compiler's relative paths start from.
        void SetSourceFolder(std::filesystem::path folder) { m_SourceFolder = std::move(folder); }
        // Takes the messages of a category away, as when they stop being true.
        void RemoveCategory(const std::string& category);

    private:
        struct Row {
            LogEntry Entry;
            u32 Count = 1;
            // Where the message points, if it does.
            std::string File;
            u32 Line = 0;
        };

        void DrawToolbar();
        void DrawRows();
        bool Passes(const Row& row) const;

        std::filesystem::path m_SourceFolder;
        std::vector<LogEntry> m_Incoming;
        std::deque<Row> m_Rows;
        // Messages of each level since the last clear, repeats included.
        std::array<u32, 3> m_Counts{};
        std::array<bool, 3> m_ShowLevel{true, true, true};
        std::string m_Filter;
        // Index into m_Rows; -1 for none.
        i64 m_Selected = -1;
        bool m_ScrollToBottom = false;
    };

} // namespace VKE::Editor
