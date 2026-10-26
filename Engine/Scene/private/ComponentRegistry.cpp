#include "scene/ComponentRegistry.h"
#include "scene/Component.h"

#include <cstring>
#include <vector>

namespace VKE::Scene {

    namespace {
        std::vector<ComponentTypeInfo>& GetTypes() {
            static std::vector<ComponentTypeInfo> types;
            return types;
        }
    } // namespace

    const ComponentTypeInfo* ComponentRegistry::Find(ComponentTypeID id) {
        const std::vector<ComponentTypeInfo>& types = GetTypes();
        return id < types.size() ? &types[id] : nullptr;
    }

    const ComponentTypeInfo* ComponentRegistry::Find(std::string_view name) {
        for (const ComponentTypeInfo& type : GetTypes()) {
            if (name == type.Name) return &type;
        }
        return nullptr;
    }

    ComponentTypeID ComponentRegistry::Add(ComponentTypeInfo info) {
        std::vector<ComponentTypeInfo>& types = GetTypes();
        for (const ComponentTypeInfo& type : types) {
            VKE_CORE_ASSERT(std::strcmp(type.Name, info.Name) != 0, "Two component types share a name");
        }

        info.ID = static_cast<ComponentTypeID>(types.size());
        types.push_back(std::move(info));
        return types.back().ID;
    }

} // namespace VKE::Scene
