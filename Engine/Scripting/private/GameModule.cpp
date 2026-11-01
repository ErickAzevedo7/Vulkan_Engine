#include "scripting/GameModule.h"
#include "ScriptComponent.h"
#include "ScriptType.h"

#include <assets/AssetTypes.h>
#include <core/Log.h>
#include <core/ObjectID.h>
#include <math/MathTypes.h>
#include <platform/DynamicLibrary.h>
#include <platform/GuardedCall.h>

#include <vke/Object.h>
#include <vke/detail/ApiBindings.h>
#include <vke/detail/GameInfo.h>

#include <cstddef>
#include <fstream>
#include <unordered_map>

namespace VKE::Scripting {

    struct PreparedGame {
        Iunique<Platform::DynamicLibrary> Library;
        std::filesystem::path Copy;
        std::filesystem::path Source;
        const vke::detail::GameInfo* Game = nullptr;
    };

    namespace {
        // Fields of these types are read as the engine's own.
        static_assert(sizeof(vke::Vec3) == sizeof(Vec3) && sizeof(vke::Quat) == sizeof(Quat));
        static_assert(offsetof(Quat, x) == offsetof(vke::Quat, x) && offsetof(Quat, w) == offsetof(vke::Quat, w));
        static_assert(sizeof(vke::PrefabRef) == sizeof(Assets::AssetID));
        static_assert(sizeof(vke::MeshRef) == sizeof(Assets::AssetID));
        static_assert(sizeof(vke::MaterialRef) == sizeof(Assets::AssetID));
        static_assert(sizeof(vke::SceneRef) == sizeof(Assets::AssetID));

        constexpr const char* kLoadFunction = "VkeLoadGame";
        // Exists only while a DLL loads: finding it means the program ended in the middle of that.
        constexpr const char* kLoadingFile = "Loading.txt";
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
                if (it->path().filename() != kLoadingFile) std::filesystem::remove(it->path(), ignored);
            }
        }

        // Unloading runs the destructors of the game's globals.
        void UnloadLibrary(Iunique<Platform::DynamicLibrary>& library, const std::filesystem::path& source) {
            std::string fault;
            bool survived = Platform::GuardedCall(
                [](void* data) { static_cast<Iunique<Platform::DynamicLibrary>*>(data)->reset(); }, &library, fault);
            if (survived) return;
            VKE_CORE_ERROR("Scripts: '{}' crashed while unloading: {}", source.u8string(), fault);
            library.release();
        }

        void Discard(Iunique<PreparedGame>& prepared) {
            if (!prepared) return;
            UnloadLibrary(prepared->Library, prepared->Source);
            std::error_code ignored;
            std::filesystem::remove(prepared->Copy, ignored);
            prepared.reset();
        }
    } // namespace

    GameModule::GameModule() = default;

    GameModule::~GameModule() {
        Discard(m_Prepared);
        Unload();
    }

    bool GameModule::Load(const std::filesystem::path& library, const std::filesystem::path& workFolder) {
        Unload();
        std::error_code ec;
        if (std::filesystem::remove(workFolder / kLoadingFile, ec)) {
            VKE_CORE_ERROR("Scripts: '{}' took the program down the last time it was loaded; it's left out this time",
                           library.u8string());
            return false;
        }
        if (!Prepare(library, workFolder)) return false;
        Switch();
        return true;
    }

    bool GameModule::Prepare(const std::filesystem::path& library, const std::filesystem::path& workFolder) {
        Discard(m_Prepared);

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

        // Loading runs the constructors of the game's globals.
        std::ofstream(workFolder / kLoadingFile).put('\n');
        Iunique<Platform::DynamicLibrary> loaded;
        const vke::detail::GameInfo* game = nullptr;
        LoadFunction load = nullptr;
        std::string fault;
        struct LoadCall {
            const std::filesystem::path& Copy;
            Iunique<Platform::DynamicLibrary>& Loaded;
            LoadFunction& Load;
            const vke::detail::GameInfo*& Game;
        } call{copy, loaded, load, game};
        bool survived = Platform::GuardedCall(
            [](void* data) {
                LoadCall& call = *static_cast<LoadCall*>(data);
                call.Loaded = Platform::DynamicLibrary::Load(call.Copy);
                if (call.Loaded) call.Load = reinterpret_cast<LoadFunction>(call.Loaded->GetSymbol(kLoadFunction));
                // Lives as long as the process: a game can keep calling through it until unloaded.
                static const vke::detail::ApiTable s_Api = vke::detail::MakeApiTable();
                if (call.Load) call.Game = call.Load(&s_Api);
            },
            &call, fault);
        std::filesystem::remove(workFolder / kLoadingFile, ec);
        if (!survived) {
            VKE_CORE_ERROR("Scripts: '{}' crashed while loading: {}", library.u8string(), fault);
            loaded.release();
            return false;
        }
        if (!load) {
            // Windows refuses a library whose globals crash, which ends up here too.
            VKE_CORE_ERROR(loaded ? "Scripts: '{}' isn't a game built with the SDK"
                                  : "Scripts: '{}' can't be loaded; a global variable's constructor may have crashed",
                           library.u8string());
            loaded.reset();
            std::filesystem::remove(copy, ec);
            return false;
        }

        if (game->ApiVersion != vke::detail::kApiVersion) {
            VKE_CORE_ERROR("Scripts: '{}' was built with another version of the SDK; build it again",
                           library.u8string());
            loaded.reset();
            std::filesystem::remove(copy, ec);
            return false;
        }

        m_Prepared = MakeIunique<PreparedGame>();
        m_Prepared->Library = std::move(loaded);
        m_Prepared->Copy = std::move(copy);
        m_Prepared->Source = library;
        m_Prepared->Game = game;
        return true;
    }

    bool GameModule::WillHaveScript(const std::string& name) const {
        if (!m_Prepared) return false;
        for (unsigned i = 0; i < m_Prepared->Game->ScriptCount; ++i) {
            if (name == m_Prepared->Game->Scripts[i].Name) return true;
        }
        return false;
    }

    void GameModule::Switch() {
        if (!m_Prepared) return;
        Iunique<PreparedGame> prepared = std::move(m_Prepared);
        Unload();

        const vke::detail::GameInfo* game = prepared->Game;
        m_Library = std::move(prepared->Library);
        m_RunningCopy = prepared->Copy;
        for (unsigned i = 0; i < game->ScriptCount; ++i) {
            const vke::detail::ScriptTypeInfo& info = game->Scripts[i];
            auto type = MakeIunique<ScriptType>();
            type->Name = info.Name;
            type->Create = info.Create;
            type->Destroy = info.Destroy;
            type->ToScript = info.ToScript;
            type->Size = info.Size;
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
            // Components take their ID from the type when they are made, these first of all.
            type->ID = Scene::ComponentRegistry::FindOrAddMissing(type->Name).ID;
            auto defaults = MakeIunique<ScriptComponent>(*type);
            if (defaults->IsBroken()) {
                VKE_CORE_ERROR("Scripts: script '{}' can't be used, and objects keep its values until it can",
                               type->Name);
                continue;
            }
            type->DefaultInstance = defaults->GetInstance();
            entry.Defaults = std::move(defaults);

            type->ID = Scene::ComponentRegistry::RegisterExternal(std::move(entry));
            if (type->ID == Scene::kInvalidComponentType) {
                VKE_CORE_ERROR("Scripts: the name of script '{}' is taken by a component of the engine", type->Name);
                continue;
            }
            m_Types.push_back(std::move(type));
        }
        VKE_CORE_INFO("Scripts: loaded {} script types from '{}'", m_Types.size(), prepared->Source.u8string());
    }

    void GameModule::Unload() {
        if (!m_Library) return;
        for (const Iunique<ScriptType>& type : m_Types) {
            Scene::ComponentRegistry::ReleaseExternal(type->ID);
        }
        m_Types.clear();
        UnloadLibrary(m_Library, m_RunningCopy);

        std::error_code ignored;
        std::filesystem::remove(m_RunningCopy, ignored);
        m_RunningCopy.clear();
    }

} // namespace VKE::Scripting
