#include "renderer/WorldDraw.h"

namespace VKE::Renderer {

    void WorldDrawList::Clear() {
        Vertices.clear();
        Indices.clear();
        Commands.clear();
        m_Style = {};
    }

    WorldCommand& WorldDrawList::GetCommand(WorldPrimitive primitive) {
        if (Commands.empty() || Commands.back().Primitive != primitive || Commands.back().Style != m_Style) {
            WorldCommand command;
            command.FirstIndex = static_cast<u32>(Indices.size());
            command.Primitive = primitive;
            command.Style = m_Style;
            Commands.push_back(command);
        }
        return Commands.back();
    }

    void WorldDrawList::AddLine(const Vec3& from, const Vec3& to, u32 color, f32 width) {
        WorldCommand& command = GetCommand(WorldPrimitive::Lines);
        u32 base = static_cast<u32>(Vertices.size());
        Vertices.push_back({from, to, Vec2(-1.0f, 0.0f), color, width});
        Vertices.push_back({from, to, Vec2(1.0f, 0.0f), color, width});
        Vertices.push_back({to, from, Vec2(1.0f, 1.0f), color, width});
        Vertices.push_back({to, from, Vec2(-1.0f, 1.0f), color, width});
        Indices.insert(Indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        command.IndexCount += 6;
    }

    void WorldDrawList::AddTriangle(const Vec3& a, const Vec3& b, const Vec3& c, u32 color) {
        WorldCommand& command = GetCommand(WorldPrimitive::Triangles);
        u32 base = static_cast<u32>(Vertices.size());
        Vertices.push_back({a, a, Vec2(0.0f), color, 0.0f});
        Vertices.push_back({b, b, Vec2(0.0f), color, 0.0f});
        Vertices.push_back({c, c, Vec2(0.0f), color, 0.0f});
        Indices.insert(Indices.end(), {base, base + 1, base + 2});
        command.IndexCount += 3;
    }

    void WorldDrawList::AddQuad(const Vec3 corners[4], u32 color, const Vec2& uvMin, const Vec2& uvMax) {
        WorldCommand& command = GetCommand(WorldPrimitive::Triangles);
        u32 base = static_cast<u32>(Vertices.size());
        Vertices.push_back({corners[0], corners[0], Vec2(uvMin.x, uvMin.y), color, 0.0f});
        Vertices.push_back({corners[1], corners[1], Vec2(uvMax.x, uvMin.y), color, 0.0f});
        Vertices.push_back({corners[2], corners[2], Vec2(uvMax.x, uvMax.y), color, 0.0f});
        Vertices.push_back({corners[3], corners[3], Vec2(uvMin.x, uvMax.y), color, 0.0f});
        Indices.insert(Indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        command.IndexCount += 6;
    }

} // namespace VKE::Renderer
