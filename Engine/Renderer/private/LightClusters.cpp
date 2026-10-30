#include "LightClusters.h"

#include <algorithm>
#include <cmath>

namespace VKE::Renderer {

    namespace {
        constexpr f32 kHalfPi = 1.57079632679f;

        struct Sphere {
            Vec3 Center;
            f32 Radius;
        };

        Vec3 SafeNormalize(const Vec3& v) {
            f32 length = glm::length(v);
            return length > 0.0f ? v / length : Vec3(0.0f);
        }

        // The smallest sphere around what a light reaches. A spot light reaches a slice of the
        // sphere of its range, whose rim is the widest part of the cone.
        Sphere GetBounds(const LightDesc& light) {
            Vec3 direction = SafeNormalize(light.Direction);
            f32 angle = light.OuterConeAngle;
            if (light.Type != LightType::Spot || angle >= kHalfPi || direction == Vec3(0.0f)) {
                return {light.Position, light.Range};
            }

            f32 cosine = std::cos(angle);
            if (cosine > 0.7071f) {
                // Narrow: the sphere through the tip and the rim.
                f32 radius = light.Range / (2.0f * cosine);
                return {light.Position + direction * radius, radius};
            }
            // Wide: the sphere around the rim.
            return {light.Position + direction * (light.Range * cosine), light.Range * std::sin(angle)};
        }
    } // namespace

    void LightClusters::AddLight(const LightDesc& light) {
        GpuLight gpuLight;
        gpuLight.PositionAndRange = Vec4(light.Position, light.Range);
        gpuLight.DirectionAndType = Vec4(SafeNormalize(light.Direction), static_cast<f32>(light.Type));
        gpuLight.ColorAndIntensity = Vec4(light.Color, light.Intensity);
        gpuLight.SpotAndShadow = Vec4(std::cos(light.InnerConeAngle), std::cos(light.OuterConeAngle), -1.0f, 0.0f);
        m_Lights.push_back(gpuLight);
        m_Sources.push_back(&light);
    }

    void LightClusters::Build(const std::vector<LightDesc>& lights, const RenderView& view, const Mat4& projection,
                              u32 width, u32 height, FrameUniforms& frame) {
        m_Lights.clear();
        m_Sources.clear();
        m_Hits.clear();
        m_Clusters.assign(kClusterColumns * kClusterRows * kClusterSlices, GpuCluster{});

        // A slice's depth grows by the same factor each step, so the thin slices are near the
        // camera, where lights are told apart on screen.
        f32 nearPlane = std::max(view.NearPlane, 1e-3f);
        f32 farPlane = std::max(view.FarPlane, nearPlane * 2.0f);
        f32 logRange = std::log(farPlane / nearPlane);
        for (u32 slice = 0; slice <= kClusterSlices; ++slice) {
            m_SliceDepths[slice] =
                nearPlane * std::exp(logRange * static_cast<f32>(slice) / static_cast<f32>(kClusterSlices));
        }
        m_SliceScale = static_cast<f32>(kClusterSlices) / logRange;
        m_SliceBias = -m_SliceScale * std::log(nearPlane);

        for (const LightDesc& light : lights) {
            if (light.Type == LightType::Directional) AddLight(light);
        }
        u32 directionalCount = static_cast<u32>(m_Lights.size());

        f32 tanX = 1.0f / std::abs(projection[0][0]);
        f32 tanY = 1.0f / std::abs(projection[1][1]);
        for (const LightDesc& light : lights) {
            if (light.Type == LightType::Directional) continue;
            Sphere bounds = GetBounds(light);
            Vec3 center = Vec3(view.View * Vec4(bounds.Center, 1.0f));
            u32 index = static_cast<u32>(m_Lights.size());
            if (FindClusters(center, bounds.Radius, index, nearPlane, farPlane, tanX, tanY)) AddLight(light);
        }

        // Each cluster's list starts where the one before it ends.
        u32 offset = 0;
        for (GpuCluster& cluster : m_Clusters) {
            cluster.Offset = offset;
            offset += cluster.Count;
            cluster.Count = 0;
        }
        m_Indices.resize(offset);
        for (const Hit& hit : m_Hits) {
            GpuCluster& cluster = m_Clusters[hit.Cluster];
            m_Indices[cluster.Offset + cluster.Count++] = hit.Light;
        }

        frame.LightCounts = UVec4(directionalCount, 0, 0, 0);
        frame.ClusterCounts = UVec4(kClusterColumns, kClusterRows, kClusterSlices, 0);
        frame.ClusterScale = Vec4(static_cast<f32>(kClusterColumns) / static_cast<f32>(width),
                                  static_cast<f32>(kClusterRows) / static_cast<f32>(height), m_SliceScale, m_SliceBias);
    }

    bool LightClusters::FindClusters(const Vec3& center, f32 radius, u32 light, f32 nearPlane, f32 farPlane, f32 tanX,
                                     f32 tanY) {
        f32 depth = -center.z;
        if (depth + radius <= nearPlane || depth - radius >= farPlane) return false;

        auto sliceOf = [&](f32 value) {
            f32 slice = std::log(std::clamp(value, nearPlane, farPlane)) * m_SliceScale + m_SliceBias;
            return std::min(static_cast<u32>(std::max(slice, 0.0f)), kClusterSlices - 1);
        };

        bool found = false;
        f32 squaredRadius = radius * radius;
        u32 lastSlice = sliceOf(depth + radius);
        for (u32 slice = sliceOf(depth - radius); slice <= lastSlice; ++slice) {
            f32 sliceNear = m_SliceDepths[slice];
            f32 sliceFar = m_SliceDepths[slice + 1];

            // The range of the screen the sphere can cover inside this slice. Over a slice's depth
            // a point moves on screen the most at the slice's two ends.
            auto toScreen = [&](f32 low, f32 high, f32 tangent, f32& outMin, f32& outMax) {
                outMin = std::min(low / sliceNear, low / sliceFar) / tangent;
                outMax = std::max(high / sliceNear, high / sliceFar) / tangent;
            };
            f32 minX, maxX, minY, maxY;
            toScreen(center.x - radius, center.x + radius, tanX, minX, maxX);
            toScreen(center.y - radius, center.y + radius, tanY, minY, maxY);
            if (minX > 1.0f || maxX < -1.0f || minY > 1.0f || maxY < -1.0f) continue;

            // Rows go down the screen, as the shader counts them.
            auto toCell = [](f32 position, u32 count) {
                f32 cell = position * 0.5f * static_cast<f32>(count);
                return std::min(static_cast<u32>(std::max(cell, 0.0f)), count - 1);
            };
            u32 firstColumn = toCell(minX + 1.0f, kClusterColumns);
            u32 lastColumn = toCell(maxX + 1.0f, kClusterColumns);
            u32 firstRow = toCell(1.0f - maxY, kClusterRows);
            u32 lastRow = toCell(1.0f - minY, kClusterRows);

            // Distance from the sphere's center to a cell's box, along one axis of the view.
            auto gap = [](f32 value, f32 low, f32 high) { return value - std::clamp(value, low, high); };
            f32 depthGap = gap(depth, sliceNear, sliceFar);

            for (u32 row = firstRow; row <= lastRow; ++row) {
                f32 top = (1.0f - 2.0f * static_cast<f32>(row) / kClusterRows) * tanY;
                f32 bottom = (1.0f - 2.0f * static_cast<f32>(row + 1) / kClusterRows) * tanY;
                f32 rowGap = gap(center.y, std::min(bottom * sliceNear, bottom * sliceFar),
                                 std::max(top * sliceNear, top * sliceFar));

                for (u32 column = firstColumn; column <= lastColumn; ++column) {
                    f32 left = (2.0f * static_cast<f32>(column) / kClusterColumns - 1.0f) * tanX;
                    f32 right = (2.0f * static_cast<f32>(column + 1) / kClusterColumns - 1.0f) * tanX;
                    f32 columnGap = gap(center.x, std::min(left * sliceNear, left * sliceFar),
                                        std::max(right * sliceNear, right * sliceFar));

                    if (columnGap * columnGap + rowGap * rowGap + depthGap * depthGap > squaredRadius) continue;

                    u32 cluster = column + kClusterColumns * (row + kClusterRows * slice);
                    m_Hits.push_back({cluster, light});
                    ++m_Clusters[cluster].Count;
                    found = true;
                }
            }
        }
        return found;
    }

} // namespace VKE::Renderer
