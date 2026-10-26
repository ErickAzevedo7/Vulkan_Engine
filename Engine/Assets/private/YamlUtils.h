#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>

#include <yaml-cpp/yaml.h>

#include <string>

namespace VKE::Assets {

    // Reads `node[key]` as T, or returns `fallback` when it is missing or has the wrong type.
    template<typename T>
    T ReadYaml(const YAML::Node& node, const char* key, const T& fallback) {
        YAML::Node value = node[key];
        if (!value || !value.IsScalar()) return fallback;
        try {
            return value.as<T>();
        } catch (const YAML::Exception&) {
            return fallback;
        }
    }

    // Reads a sequence of N numbers into the first N components of a vector type.
    template<typename VecT, int N>
    VecT ReadYamlVector(const YAML::Node& node, const char* key, const VecT& fallback) {
        YAML::Node value = node[key];
        if (!value || !value.IsSequence() || value.size() != N) return fallback;
        VecT result = fallback;
        try {
            for (int i = 0; i < N; ++i) {
                result[i] = value[i].as<f32>();
            }
        } catch (const YAML::Exception&) {
            return fallback;
        }
        return result;
    }

} // namespace VKE::Assets
