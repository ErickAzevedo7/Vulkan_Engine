#include "scripting/GameModule.h"
#include "ScriptComponent.h"
#include "ScriptType.h"

#include <assets/AssetTypes.h>
#include <core/Log.h>
#include <core/ObjectID.h>
#include <math/MathTypes.h>
#include <platform/DynamicLibrary.h>

#include <vke/Object.h>
#include <vke/detail/ApiBindings.h>
#include <vke/detail/GameInfo.h>

#include <cstddef>
#include <unordered_map>

namespace VKE::Scripting {

    namespace {
        // Fields of these types are read as the engine's own.
        static_assert(sizeof(vke::Vec3) == sizeof(Vec3) && sizeof(vke::Quat) == sizeof(Quat));
        static_assert(offsetof(Quat, x) == offsetof(vke::Quat, x) && offsetof(Quat, w) == offsetof(vke::Quat, w));
        static_assert(sizeof(vke::PrefabRef) == sizeof(Assets::AssetID));
        static_assert(sizeof(vke::MeshRef) == sizeof(Assets::AssetID));
        static_assert(sizeof(vke::MaterialRef) == sizeof(Assets::AssetID));
        static_assert(sizeof(vke::SceneRef) == sizeof(Assets::AssetID));

        constexpr const char* kLoadFunction = "VkeLoadGame";
        using LoadFunction = const vke::detail::GameInfo* (*)(const vke::detail::ApiTable*);

        Scene::PropertyType Convert(vke::detail::PropertyType type) {
            using From = vke::detail::PropertyType;
            using To = Scene::PropertyType;
            switch (type) {
            case From::Bool: return To::Bool;
            case From::I32: return To::I32;
            case From::F32: return To::F32;
            case From::Vec3: return To::Vec3;
            case From::Quat: return To::Quat;
            case From::Enum: return To::Enum;
            case From::Struct: return To::Struct;
            case From::Object: return To::Object;
            case From::Prefab: return To::Prefab;
            case From::MeshAsset: return To::MeshAsset;
            case From::MaterialAsset: return To::MaterialAsset;
            case From::SceneAsset: return To::SceneAsset;
            }
            return To::Bool;
        }

        Scene::PropertyDisplay Convert(vke::detail::PropertyDisplay display) {
            switch (display) {
            case vke::detail::PropertyDisplay::Color: return Scene::PropertyDisplay::Color;
            case vke::detail::PropertyDisplay::Angle: return Scene::PropertyDisplay::Angle;
            case vke::detail::PropertyDisplay::Default: break;
            }
            return Scene::PropertyDisplay::Default;
        }

        // Rewrites the DLL's tables as the engine's, in storage owned by `type`.
        class TableConverter {
        public:
            explicit TableConverter(ScriptType& type) : m_Type(type) {}

            Scene::StructInfo ConvertStruct(const vke::detail::StructInfo& from) {
                std::vector<Scene::PropertyInfo>& properties = m_Type.PropertyLists.emplace_back();
                properties.reserve(from.Count);
                for (unsigned i = 0; i < from.Count; ++i) {
                    const vke::detail::PropertyInfo& property = from.Properties[i];
                    properties.push_back({property.Name, Convert(property.Type), property.Access,
                                          property.Enum ? ConvertEnum(*property.Enum) : nullptr,
                                          property.Struct ? ConvertInner(*property.Struct) : nullptr, property.Min,
                                          property.Max, Convert(property.Display)});
                }
                return {from.Name, properties.empty() ? nullptr : properties.data(), from.Count};
            }

        private:
            const Scene::StructInfo* ConvertInner(const vke::detail::StructInfo& from) {
                auto [it, added] = m_Structs.try_emplace(&from, nullptr);
                if (added) {
                    Scene::StructInfo converted = ConvertStruct(from);
                    m_Type.Structs.push_back(converted);
                    m_Structs[&from] = &m_Type.Structs.back();
                }
                return m_Structs[&from];
            }

            const Scene::EnumInfo* ConvertEnum(const vke::detail::EnumInfo& from) {
                auto [it, added] = m_Enums.try_emplace(&from, nullptr);
                if (added) {
                    std::vector<Scene::EnumValue>& values = m_Type.EnumValues.emplace_back();
                    for (unsigned i = 0; i < from.Count; ++i) {
                        values.push_back({from.Values[i].Name, from.Values[i].Value});
                    }
                    m_Type.Enums.push_back({from.Name, values.data(), from.Count, from.Size});
                    it->second = &m_Type.Enums.back();
                }
                return it->second;
            }

            ScriptType& m_Type;
            std::unordered_map<const vke::detail::StructInfo*, const Scene::StructInfo*> m_Structs;
            std::unordered_map<const vke::detail::EnumInfo*, const Scene::EnumInfo*> m_Enums;
        };

        // Copies left by programs that ended without unloading can't be in use anymore, unless
        // another program runs the same project, and then they can't be removed either.
        void RemoveOldCopies(const std::filesystem::path& workFolder) {
            std::error_code ec;
            for (auto it = std::filesystem::directory_iterator(workFolder, ec);
                 !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
                std::error_code ignored;
                std::filesystem::remove(it->path(), ignored);
            }
        }
    } // namespace

    GameModule::GameModule() = default;

    GameModule::~GameModule() {
        Unload();
    }

    bool GameModule::Load(const std::filesystem::path& library, const std::filesystem::path& workFolder) {
        Unload();

        std::error_code ec;
        if (!std::filesystem::exists(library, ec)) {
            VKE_CORE_INFO("Scripts: no game built yet at '{}'", library.u8string());
            return false;
        }

        std::filesystem::create_directories(workFolder, ec);
        RemoveOldCopies(workFolder);
        std::filesystem::path copy = workFolder / ("Game-" + ObjectID::Generate().ToString() + ".dll");
        std::filesystem::copy_file(library, copy, ec);
        if (ec) {
            VKE_CORE_ERROR("Scripts: couldn't copy '{}': {}", library.u8string(), ec.message());
            return false;
        }

        Iunique<Platform::DynamicLibrary> loaded = Platform::DynamicLibrary::Load(copy);
        auto load = loaded ? reinterpret_cast<LoadFunction>(loaded->GetSymbol(kLoadFunction)) : nullptr;
        if (!load) {
            VKE_CORE_ERROR("Scripts: '{}' isn't a game built with the SDK", library.u8string());
            loaded.reset();
            std::filesystem::remove(copy, ec);
            return false;
        }

        // Lives as long as the process: a game can keep calling through it until it is unloaded.
        static const vke::detail::ApiTable s_Api = vke::detail::MakeApiTable();
        const vke::detail::GameInfo* game = load(&s_Api);
        if (game->ApiVersion != vke::detail::kApiVersion) {
            VKE_CORE_ERROR("Scripts: '{}' was built with another version of the SDK; build it again",
                           library.u8string());
            loaded.reset();
            std::filesystem::remove(copy, ec);
            return false;
        }

        m_Library = std::move(loaded);
        m_RunningCopy = std::move(copy);
        for (unsigned i = 0; i < game->ScriptCount; ++i) {
            const vke::detail::ScriptTypeInfo& info = game->Scripts[i];
            auto type = MakeIunique<ScriptType>();
            type->Name = info.Name;
            type->Create = info.Create;
            type->Destroy = info.Destroy;
            type->ToScript = info.ToScript;
            type->Calls = game->Calls;
            type->UsesUpdate = info.UsesUpdate;
            type->UsesLateUpdate = info.UsesLateUpdate;
            type->Properties = TableConverter(*type).ConvertStruct(*info.Properties);

            Scene::ComponentTypeInfo entry;
            entry.Name = type->Name.c_str();
            entry.Context = type.get();
            entry.Create = [](const Scene::ComponentTypeInfo& component) -> Scene::Component* {
                return new ScriptComponent(*static_cast<const ScriptType*>(component.Context));
            };
            entry.Properties = &type->Properties;
            entry.ToObject = [](Scene::Component* component) {
                return static_cast<ScriptComponent*>(component)->GetInstance();
            };
            entry.Defaults.reset(new ScriptComponent(*type));

            type->ID = Scene::ComponentRegistry::RegisterExternal(std::move(entry));
            if (type->ID == Scene::kInvalidComponentType) {
                VKE_CORE_ERROR("Scripts: the name of script '{}' is taken by a component of the engine", type->Name);
                continue;
            }
            m_Types.push_back(std::move(type));
        }
        VKE_CORE_INFO("Scripts: loaded {} script types from '{}'", m_Types.size(), library.u8string());
        return true;
    }

    void GameModule::Unload() {
        if (!m_Library) return;
        for (const Iunique<ScriptType>& type : m_Types) {
            Scene::ComponentRegistry::ReleaseExternal(type->ID);
        }
        m_Types.clear();
        m_Library.reset();

        std::error_code ignored;
        std::filesystem::remove(m_RunningCopy, ignored);
        m_RunningCopy.clear();
    }

} // namespace VKE::Scripting
