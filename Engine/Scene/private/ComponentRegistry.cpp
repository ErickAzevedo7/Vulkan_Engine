#include "scene/ComponentRegistry.h"
#include "scene/Component.h"
#include "scene/components/MissingComponent.h"

#include <cstring>
#include <deque>
#include <string>

namespace VKE::Scene {

    namespace {
        constexpr StructInfo kNoProperties = {"Missing", nullptr, 0};

        // A deque: what Find returns stays valid as types are added.
        std::deque<ComponentTypeInfo>& GetTypes() {
            static std::deque<ComponentTypeInfo> types;
            return types;
        }

        // The names of types defined outside the engine, which outlive what defined them.
        const char* KeepName(std::string_view name) {
            static std::deque<std::string> names;
            for (const std::string& kept : names) {
                if (kept == name) return kept.c_str();
            }
            return names.emplace_back(name).c_str();
        }

        ComponentTypeInfo MakeMissing(const char* name, ComponentTypeID id) {
            ComponentTypeInfo info;
            info.ID = id;
            info.Name = name;
            info.Create = [](const ComponentTypeInfo& type) -> Component* { return new MissingComponent(type.ID); };
            info.Properties = &kNoProperties;
            info.ToObject = [](Component* component) -> void* { return component; };
            info.Defaults.reset(new MissingComponent(id));
            info.Missing = true;
            return info;
        }
    } // namespace

    ComponentTypeID ComponentRegistry::RegisterExternal(ComponentTypeInfo info) {
        info.Name = KeepName(info.Name);
        const ComponentTypeInfo& type = FindOrAddMissing(info.Name);
        if (!type.Missing) return kInvalidComponentType;

        info.ID = type.ID;
        ComponentTypeID id = info.ID;
        GetTypes()[id] = std::move(info);
        return id;
    }

    void ComponentRegistry::ReleaseExternal(ComponentTypeID id) {
        ComponentTypeInfo& type = GetTypes()[id];
        type = MakeMissing(type.Name, id);
    }

    const ComponentTypeInfo& ComponentRegistry::FindOrAddMissing(std::string_view name) {
        if (const ComponentTypeInfo* type = Find(name)) return *type;
        std::deque<ComponentTypeInfo>& types = GetTypes();
        return types.emplace_back(MakeMissing(KeepName(name), static_cast<ComponentTypeID>(types.size())));
    }

    u32 ComponentRegistry::GetCount() {
        return static_cast<u32>(GetTypes().size());
    }

    const ComponentTypeInfo* ComponentRegistry::Find(ComponentTypeID id) {
        const std::deque<ComponentTypeInfo>& types = GetTypes();
        return id < types.size() ? &types[id] : nullptr;
    }

    const ComponentTypeInfo* ComponentRegistry::Find(std::string_view name) {
        for (const ComponentTypeInfo& type : GetTypes()) {
            if (name == type.Name) return &type;
        }
        return nullptr;
    }

    ComponentTypeID ComponentRegistry::Add(ComponentTypeInfo info) {
        std::deque<ComponentTypeInfo>& types = GetTypes();
        for (const ComponentTypeInfo& type : types) {
            VKE_CORE_ASSERT(std::strcmp(type.Name, info.Name) != 0, "Two component types share a name");
        }

        info.ID = static_cast<ComponentTypeID>(types.size());
        types.push_back(std::move(info));
        return types.back().ID;
    }

} // namespace VKE::Scene
