#pragma once

#include "LogCapture.h"

#include <core/Types.h>

#include <array>
#include <deque>
#include <string>
#include <vector>

namespace VKE::Editor {

    // The engine's log, newest at the bottom. The same message logged several times in a row
    // shows once, with a count. Messages can be filtered by level and text, and copied.
    class ConsolePanel {
    public:
        // The window's title, with its icon.
        static const char* GetWindowName();

        // Takes what was logged since the last frame. Call every frame, shown or not.
        void Update();
        void Draw(bool* open);

        // The newest message, or null when there is none.
        const LogEntry* GetLast() const;

    private:
        struct Row {
            LogEntry Entry;
            u32 Count = 1;
        };

        void DrawToolbar();
        void DrawRows();
        bool Passes(const Row& row) const;

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
