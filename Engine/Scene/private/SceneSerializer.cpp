#include "scene/SceneSerializer.h"
#include "scene/ComponentRegistry.h"
#include "scene/GameObject.h"
#include "scene/Prefab.h"
#include "scene/Properties.h"
#include "scene/Scene.h"
#include "scene/components/CameraComponent.h"
#include "scene/components/MissingComponent.h"

#include <assets/AssetManager.h>
#include <core/Log.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace VKE::Scene {

    namespace {
        // Parents before children, siblings in order, so a loader can build the hierarchy in one pass.
        void CollectPreOrder(GameObject& object, std::vector<GameObject*>& out) {
            out.push_back(&object);
            for (GameObject* child : object.GetChildren()) {
                CollectPreOrder(*child, out);
            }
        }

        using MaterialList = std::vector<Handle<Assets::MaterialData>>;

        template<typename T>
        const T& As(const void* value) {
            return *static_cast<const T*>(value);
        }

        template<typename T>
        T& As(void* value) {
            return *static_cast<T*>(value);
        }

        // Writes the properties of `object` that differ from `defaults` as one YAML map. Loading
        // starts from the same values, a new component's or a prefab's, so what's left out comes
        // back as it was.
        class PropertyWriter {
        public:
            PropertyWriter(YAML::Emitter& out, const Assets::AssetManager& assets, const std::string& owner)
                : m_Out(out), m_Assets(assets), m_Owner(owner) {}

            void Write(const StructInfo& type, const void* object, const void* defaults) {
                // Nothing to write fits on the key's line, as {}.
                if (AreEqual(type, object, defaults)) {
                    m_Out << YAML::Flow << YAML::BeginMap << YAML::EndMap;
                    return;
                }
                m_Out << YAML::BeginMap;
                for (const PropertyInfo& property : type) {
                    const void* value = property.Get(object);
                    if (AreEqual(property, value, property.Get(defaults))) continue;
                    WriteProperty(property, value, property.Get(defaults));
                }
                m_Out << YAML::EndMap;
            }

        private:
            void WriteProperty(const PropertyInfo& property, const void* value, const void* defaults) {
                switch (property.Type) {
                case PropertyType::Bool:
                    Key(property) << As<bool>(value);
                    break;
                case PropertyType::I32:
                    Key(property) << As<i32>(value);
                    break;
                case PropertyType::Object: {
                    // Nothing is an empty string: the values compared with can have something.
                    ObjectID id = As<ObjectID>(value);
                    Key(property) << YAML::DoubleQuoted << (id.IsValid() ? id.ToString() : std::string());
                    break;
                }
                case PropertyType::Prefab:
                case PropertyType::MeshAsset:
                case PropertyType::MaterialAsset:
                case PropertyType::SceneAsset: {
                    Assets::AssetID id = As<Assets::AssetID>(value);
                    Key(property) << YAML::DoubleQuoted << (id.IsValid() ? id.ToString() : std::string());
                    break;
                }
                case PropertyType::F32:
                    Key(property) << As<f32>(value);
                    break;
                case PropertyType::Vec3: {
                    const Vec3& v = As<Vec3>(value);
                    Key(property) << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
                    break;
                }
                case PropertyType::Quat: {
                    // In x, y, z, w order.
                    const Quat& q = As<Quat>(value);
                    Key(property) << YAML::Flow << YAML::BeginSeq << q.x << q.y << q.z << q.w << YAML::EndSeq;
                    break;
                }
                case PropertyType::Enum: {
                    u32 number = property.Enum->Read(value);
                    for (const EnumValue* it = property.Enum->Values; it != property.Enum->Values + property.Enum->Count;
                         ++it) {
                        if (it->Value == number) {
                            Key(property) << it->Name;
                            return;
                        }
                    }
                    VKE_CORE_WARN("SceneSerializer: {} on '{}' has a value {} doesn't name, and isn't saved",
                                  property.Name, m_Owner, property.Enum->Name);
                    break;
                }
                case PropertyType::Struct:
                    Key(property);
                    Write(*property.Struct, value, defaults);
                    break;
                case PropertyType::Mesh: {
                    // No mesh is an empty string: the values compared with can have one.
                    Handle<Assets::MeshData> mesh = As<Handle<Assets::MeshData>>(value);
                    Assets::AssetID id = m_Assets.GetID(mesh);
                    if (mesh.IsValid() && !id.IsValid()) {
                        WarnMadeInCode(property);
                        break;
                    }
                    Key(property) << YAML::DoubleQuoted << (id.IsValid() ? id.ToString() : std::string());
                    break;
                }
                case PropertyType::Materials: {
                    // Materials made in code are written as empty strings, so the others keep their slots.
                    Key(property) << YAML::Flow << YAML::BeginSeq;
                    for (Handle<Assets::MaterialData> material : As<MaterialList>(value)) {
                        Assets::AssetID id = m_Assets.GetID(material);
                        if (material.IsValid() && !id.IsValid()) WarnMadeInCode(property);
                        m_Out << YAML::DoubleQuoted << (id.IsValid() ? id.ToString() : std::string());
                    }
                    m_Out << YAML::EndSeq;
                    break;
                }
                }
            }

            YAML::Emitter& Key(const PropertyInfo& property) {
                return m_Out << YAML::Key << property.Name << YAML::Value;
            }

            void WarnMadeInCode(const PropertyInfo& property) {
                VKE_CORE_WARN("SceneSerializer: {} on '{}' uses an asset made in code, which isn't saved",
                              property.Name, m_Owner);
            }

            YAML::Emitter& m_Out;
            const Assets::AssetManager& m_Assets;
            const std::string& m_Owner;
        };

        // Sets the properties of `object` from one YAML map. Missing properties keep the value the
        // object already has; malformed ones too, with a warning.
        class PropertyReader {
        public:
            PropertyReader(Assets::AssetManager& assets, const std::string& owner) : m_Assets(assets), m_Owner(owner) {}

            void Read(const StructInfo& type, const YAML::Node& node, void* object, const std::string& path) {
                if (!node || node.IsNull()) return;
                if (!node.IsMap()) {
                    Warn(path, "isn't a map of properties");
                    return;
                }

                for (const PropertyInfo& property : type) {
                    YAML::Node value = node[property.Name];
                    if (!value) continue;
                    ReadProperty(property, value, property.Get(object), path + property.Name);
                }

                // A name the type doesn't have is usually a property that was renamed or removed.
                for (const auto& item : node) {
                    std::string name = item.first.Scalar();
                    bool known = false;
                    for (const PropertyInfo& property : type) {
                        known |= name == property.Name;
                    }
                    if (!known) Warn(path + name, "isn't a property of " + std::string(type.Name) + " and is ignored");
                }
            }

        private:
            void ReadProperty(const PropertyInfo& property, const YAML::Node& node, void* value, const std::string& path) {
                switch (property.Type) {
                case PropertyType::Bool:
                    ReadScalar(node, As<bool>(value), path);
                    break;
                case PropertyType::I32:
                    ReadScalar(node, As<i32>(value), path);
                    break;
                case PropertyType::Object: {
                    std::string text;
                    if (ReadScalar(node, text, path)) As<ObjectID>(value) = ObjectID::Parse(text);
                    break;
                }
                case PropertyType::Prefab:
                case PropertyType::MeshAsset:
                case PropertyType::MaterialAsset:
                case PropertyType::SceneAsset: {
                    std::string text;
                    if (ReadScalar(node, text, path)) As<Assets::AssetID>(value) = Assets::AssetID::Parse(text);
                    break;
                }
                case PropertyType::F32:
                    ReadScalar(node, As<f32>(value), path);
                    break;
                case PropertyType::Vec3: {
                    f32 c[3];
                    if (ReadNumbers(node, c, 3, path)) As<Vec3>(value) = Vec3(c[0], c[1], c[2]);
                    break;
                }
                case PropertyType::Quat: {
                    f32 c[4];
                    if (ReadNumbers(node, c, 4, path)) As<Quat>(value) = glm::normalize(Quat(c[3], c[0], c[1], c[2]));
                    break;
                }
                case PropertyType::Enum: {
                    std::string text;
                    if (!ReadScalar(node, text, path)) break;
                    for (const EnumValue* it = property.Enum->Values; it != property.Enum->Values + property.Enum->Count;
                         ++it) {
                        if (text == it->Name) {
                            property.Enum->Write(value, it->Value);
                            return;
                        }
                    }
                    Warn(path, "'" + text + "' isn't a value of " + property.Enum->Name);
                    break;
                }
                case PropertyType::Struct:
                    Read(*property.Struct, node, value, path + ".");
                    break;
                case PropertyType::Mesh: {
                    std::string text;
                    if (!ReadScalar(node, text, path)) break;
                    As<Handle<Assets::MeshData>>(value) = {};
                    if (text.empty()) break;
                    As<Handle<Assets::MeshData>>(value) = m_Assets.LoadMesh(Assets::AssetID::Parse(text));
                    if (!As<Handle<Assets::MeshData>>(value).IsValid()) Warn(path, "refers to a mesh that can't be loaded");
                    break;
                }
                case PropertyType::Materials: {
                    if (!node.IsSequence()) {
                        Warn(path, "isn't a list");
                        break;
                    }
                    MaterialList& materials = As<MaterialList>(value);
                    materials.clear();
                    for (const YAML::Node& item : node) {
                        std::string text = item.IsScalar() ? item.Scalar() : std::string();
                        Handle<Assets::MaterialData> material;
                        if (!text.empty()) {
                            material = m_Assets.LoadMaterial(Assets::AssetID::Parse(text));
                            if (!material.IsValid()) Warn(path, "refers to a material that can't be loaded");
                        }
                        materials.push_back(material);
                    }
                    break;
                }
                }
            }

            template<typename T>
            bool ReadScalar(const YAML::Node& node, T& value, const std::string& path) {
                if (node.IsScalar()) {
                    try {
                        value = node.as<T>();
                        return true;
                    } catch (const YAML::Exception&) {
                    }
                }
                Warn(path, "has the wrong type");
                return false;
            }

            bool ReadNumbers(const YAML::Node& node, f32* out, size_t count, const std::string& path) {
                if (!node.IsSequence() || node.size() != count) {
                    Warn(path, "needs " + std::to_string(count) + " numbers");
                    return false;
                }
                try {
                    for (size_t i = 0; i < count; ++i) {
                        out[i] = node[i].as<f32>();
                    }
                    return true;
                } catch (const YAML::Exception&) {
                    Warn(path, "has the wrong type");
                    return false;
                }
            }

            void Warn(const std::string& path, const std::string& problem) {
                VKE_CORE_WARN("SceneSerializer: {} on '{}' {}", path, m_Owner, problem);
            }

            Assets::AssetManager& m_Assets;
            const std::string& m_Owner;
        };

        ObjectID ReadID(const YAML::Node& node, const char* key) {
            YAML::Node value = node[key];
            return value && value.IsScalar() ? ObjectID::Parse(value.Scalar()) : ObjectID();
        }

        void ReadActive(const YAML::Node& node, GameObject& object) {
            YAML::Node active = node["Active"];
            if (!active || !active.IsScalar()) return;
            try {
                object.SetActive(active.as<bool>());
            } catch (const YAML::Exception&) {
            }
        }

        // What a component whose type isn't there said in the file.
        void WriteMissing(YAML::Emitter& out, const Component& component) {
            const std::string& text = static_cast<const MissingComponent&>(component).GetSavedText();
            YAML::Node node = YAML::Load(text);
            if (node.IsNull() || !node.IsDefined()) node = YAML::Node(YAML::NodeType::Map);
            out << node;
        }

        void WriteObject(YAML::Emitter& out, const GameObject& object, const Assets::AssetManager& assets,
                         bool writeParent) {
            out << YAML::BeginMap;
            out << YAML::Key << "ID" << YAML::Value << YAML::DoubleQuoted << object.GetID().ToString();
            out << YAML::Key << "Name" << YAML::Value << YAML::DoubleQuoted << object.GetName();
            if (GameObject* parent = object.GetParent(); parent && writeParent) {
                out << YAML::Key << "Parent" << YAML::Value << YAML::DoubleQuoted << parent->GetID().ToString();
            }
            out << YAML::Key << "Active" << YAML::Value << object.IsActive();

            out << YAML::Key << "Components" << YAML::Value << YAML::BeginMap;
            for (Component* component : object.GetComponents()) {
                const ComponentTypeInfo* type = ComponentRegistry::Find(component->GetTypeID());
                out << YAML::Key << type->Name << YAML::Value;
                if (type->Missing) {
                    WriteMissing(out, *component);
                    continue;
                }
                PropertyWriter writer(out, assets, object.GetName());
                writer.Write(*type->Properties, type->ToObject(component), type->ToObject(type->Defaults.get()));
            }
            out << YAML::EndMap;
            out << YAML::EndMap;
        }

        // An instance: its prefab and what differs from it, by the part's ID in the prefab.
        void WriteInstance(YAML::Emitter& out, GameObject& root, PrefabLibrary& prefabs) {
            out << YAML::BeginMap;
            out << YAML::Key << "ID" << YAML::Value << YAML::DoubleQuoted << root.GetID().ToString();
            out << YAML::Key << "Name" << YAML::Value << YAML::DoubleQuoted << root.GetName();
            out << YAML::Key << "Prefab" << YAML::Value << YAML::DoubleQuoted << root.GetPrefab().ToString();
            if (GameObject* parent = root.GetParent()) {
                out << YAML::Key << "Parent" << YAML::Value << YAML::DoubleQuoted << parent->GetID().ToString();
            }

            std::vector<GameObject*> parts;
            CollectPreOrder(root, parts);
            bool opened = false;
            for (GameObject* part : parts) {
                GameObject* pristine = prefabs.GetPart(*part);
                if (!pristine) continue;

                std::vector<std::pair<Component*, Component*>> changed;
                for (Component* component : part->GetComponents()) {
                    const ComponentTypeInfo* type = ComponentRegistry::Find(component->GetTypeID());
                    Component* original = pristine->GetComponent(component->GetTypeID());
                    if (!original) continue;
                    bool differs = type->Missing ? static_cast<MissingComponent*>(component)->GetSavedText() !=
                                                       static_cast<MissingComponent*>(original)->GetSavedText()
                                                 : !AreEqual(*type->Properties, type->ToObject(component),
                                                             type->ToObject(original));
                    if (differs) changed.emplace_back(component, original);
                }
                bool renamed = part != &root && part->GetName() != pristine->GetName();
                bool switched = part->IsActive() != pristine->IsActive();
                if (changed.empty() && !renamed && !switched) continue;

                if (!opened) out << YAML::Key << "Overrides" << YAML::Value << YAML::BeginSeq;
                opened = true;
                out << YAML::BeginMap;
                out << YAML::Key << "Part" << YAML::Value << YAML::DoubleQuoted << part->GetPrefabPart().ToString();
                if (renamed) out << YAML::Key << "Name" << YAML::Value << YAML::DoubleQuoted << part->GetName();
                if (switched) out << YAML::Key << "Active" << YAML::Value << part->IsActive();
                if (!changed.empty()) {
                    out << YAML::Key << "Components" << YAML::Value << YAML::BeginMap;
                    for (auto [component, original] : changed) {
                        const ComponentTypeInfo* type = ComponentRegistry::Find(component->GetTypeID());
                        out << YAML::Key << type->Name << YAML::Value;
                        if (type->Missing) {
                            WriteMissing(out, *component);
                            continue;
                        }
                        PropertyWriter writer(out, prefabs.GetAssets(), part->GetName());
                        writer.Write(*type->Properties, type->ToObject(component), type->ToObject(original));
                    }
                    out << YAML::EndMap;
                }
                out << YAML::EndMap;
            }
            if (opened) out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        void ReadOverrides(const YAML::Node& overrides, GameObject& root, Assets::AssetManager& assets) {
            if (!overrides || !overrides.IsSequence()) return;
            std::vector<GameObject*> parts;
            CollectPreOrder(root, parts);

            for (const YAML::Node& node : overrides) {
                if (!node.IsMap()) continue;
                ObjectID partID = ReadID(node, "Part");
                auto found = std::find_if(parts.begin(), parts.end(),
                                          [&](GameObject* part) { return part->GetPrefabPart() == partID; });
                if (found == parts.end()) {
                    VKE_CORE_WARN("SceneSerializer: an override of '{}' is for a part its prefab doesn't have anymore "
                                  "and is ignored",
                                  root.GetName());
                    continue;
                }

                GameObject& object = **found;
                if (node["Name"] && node["Name"].IsScalar()) object.SetName(node["Name"].Scalar());
                ReadActive(node, object);

                YAML::Node components = node["Components"];
                if (!components || !components.IsMap()) continue;
                for (const auto& item : components) {
                    std::string typeName = item.first.Scalar();
                    const ComponentTypeInfo* type = ComponentRegistry::Find(typeName);
                    Component* component = type ? object.GetComponent(type->ID) : nullptr;
                    if (!component) {
                        VKE_CORE_WARN("SceneSerializer: an override of '{}' is for a component '{}' it doesn't have "
                                      "and is ignored",
                                      object.GetName(), typeName);
                        continue;
                    }
                    if (type->Missing) {
                        static_cast<MissingComponent*>(component)->SetSavedText(YAML::Dump(item.second));
                        continue;
                    }
                    PropertyReader reader(assets, object.GetName());
                    reader.Read(*type->Properties, item.second, type->ToObject(component),
                                std::string(type->Name) + ".");
                    component->ApplyPropertyChanges(assets);
                }
            }
        }

        bool WriteFile(const std::string& text, const std::filesystem::path& file) {
            std::ofstream stream(file, std::ios::binary | std::ios::trunc);
            stream << text;
            if (!stream) {
                VKE_CORE_ERROR("SceneSerializer: couldn't write '{}'", file.string());
                return false;
            }
            return true;
        }

        std::string ToFileText(const YAML::Emitter& out) {
            if (!out.good()) {
                VKE_CORE_ERROR("SceneSerializer: couldn't write the objects: {}", out.GetLastError());
                return {};
            }
            return std::string(out.c_str()) + '\n';
        }
    } // namespace

    std::string SceneSerializer::ToText(const Scene& scene, PrefabLibrary& prefabs) {
        std::vector<GameObject*> objects;
        for (GameObject* root : scene.GetRootObjects()) {
            CollectPreOrder(*root, objects);
        }

        YAML::Emitter out;
        out << YAML::Comment("Written by the engine.");
        out << YAML::BeginMap;
        if (CameraComponent* camera = scene.GetMainCamera()) {
            out << YAML::Key << "MainCamera" << YAML::Value << YAML::DoubleQuoted
                << camera->GetOwner().GetID().ToString();
        }

        out << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;
        for (GameObject* object : objects) {
            // An instance whose prefab is gone is kept as plain objects.
            if (object->IsPrefabPart() && prefabs.GetRoot(object->GetPrefab())) {
                if (object->IsPrefabRoot()) WriteInstance(out, *object, prefabs);
            } else {
                WriteObject(out, *object, prefabs.GetAssets(), true);
            }
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
        return ToFileText(out);
    }

    bool SceneSerializer::Save(const Scene& scene, PrefabLibrary& prefabs, const std::filesystem::path& file) {
        std::string text = ToText(scene, prefabs);
        if (text.empty() || !WriteFile(text, file)) return false;
        VKE_CORE_INFO("SceneSerializer: saved {} objects to '{}'", scene.GetObjectCount(), file.string());
        return true;
    }

    bool SceneSerializer::Load(Scene& scene, PrefabLibrary& prefabs, const std::filesystem::path& file) {
        YAML::Node root;
        try {
            root = YAML::LoadFile(file.string());
        } catch (const YAML::Exception& error) {
            VKE_CORE_ERROR("SceneSerializer: '{}': {}", file.string(), error.what());
            return false;
        }
        return LoadObjects(scene, prefabs.GetAssets(), &prefabs, root, file.string());
    }

    bool SceneSerializer::LoadText(Scene& scene, PrefabLibrary& prefabs, const std::string& text,
                                   const std::string& name) {
        YAML::Node root;
        try {
            root = YAML::Load(text);
        } catch (const YAML::Exception& error) {
            VKE_CORE_ERROR("SceneSerializer: '{}': {}", name, error.what());
            return false;
        }
        return LoadObjects(scene, prefabs.GetAssets(), &prefabs, root, name);
    }

    std::string SceneSerializer::PrefabToText(GameObject& root, const Assets::AssetManager& assets) {
        std::vector<GameObject*> objects;
        CollectPreOrder(root, objects);

        YAML::Emitter out;
        out << YAML::Comment("Written by the engine.");
        out << YAML::BeginMap;
        out << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;
        for (const GameObject* object : objects) {
            WriteObject(out, *object, assets, object != &root);
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
        return ToFileText(out);
    }

    bool SceneSerializer::SavePrefab(GameObject& root, const Assets::AssetManager& assets,
                                     const std::filesystem::path& file) {
        std::string text = PrefabToText(root, assets);
        if (text.empty() || !WriteFile(text, file)) return false;
        VKE_CORE_INFO("SceneSerializer: saved the prefab '{}' to '{}'", root.GetName(), file.string());
        return true;
    }

    bool SceneSerializer::LoadPrefab(Scene& scene, Assets::AssetManager& assets, const std::filesystem::path& file) {
        YAML::Node root;
        try {
            root = YAML::LoadFile(file.string());
        } catch (const YAML::Exception& error) {
            VKE_CORE_ERROR("SceneSerializer: '{}': {}", file.string(), error.what());
            return false;
        }
        return LoadObjects(scene, assets, nullptr, root, file.string());
    }

    bool SceneSerializer::LoadPrefabText(Scene& scene, Assets::AssetManager& assets, const std::string& text,
                                         const std::string& name) {
        YAML::Node root;
        try {
            root = YAML::Load(text);
        } catch (const YAML::Exception& error) {
            VKE_CORE_ERROR("SceneSerializer: '{}': {}", name, error.what());
            return false;
        }
        return LoadObjects(scene, assets, nullptr, root, name);
    }

    std::string SceneSerializer::ComponentToText(Component& component, const Assets::AssetManager& assets) {
        const ComponentTypeInfo* type = ComponentRegistry::Find(component.GetTypeID());
        YAML::Emitter out;
        PropertyWriter writer(out, assets, component.GetOwner().GetName());
        writer.Write(*type->Properties, type->ToObject(&component), type->ToObject(type->Defaults.get()));
        return out.c_str();
    }

    void SceneSerializer::LoadComponentText(Component& component, Assets::AssetManager& assets, const std::string& text) {
        const ComponentTypeInfo* type = ComponentRegistry::Find(component.GetTypeID());
        PropertyReader reader(assets, component.GetOwner().GetName());
        reader.Read(*type->Properties, YAML::Load(text), type->ToObject(&component), std::string(type->Name) + ".");
        component.ApplyPropertyChanges(assets);
    }

    bool SceneSerializer::LoadObjects(Scene& scene, Assets::AssetManager& assets, PrefabLibrary* prefabs,
                                      const YAML::Node& root, const std::string& sourceName) {
        YAML::Node objectNodes = root["Objects"];
        if (!root.IsMap() || !objectNodes || !objectNodes.IsSequence()) {
            VKE_CORE_ERROR("SceneSerializer: '{}' is not a scene file", sourceName);
            return false;
        }

        bool hadMainCamera = scene.GetMainCamera() != nullptr;

        // First every object, so parents can be found by ID whatever their place in the file.
        struct Loaded {
            YAML::Node Node;
            GameObject* Object;
            bool IsInstance;
        };
        std::vector<Loaded> loaded;
        std::unordered_map<ObjectID, GameObject*> byID;
        std::unordered_set<ObjectID> skipped;
        for (const YAML::Node& node : objectNodes) {
            if (!node.IsMap()) continue;
            ObjectID id = ReadID(node, "ID");
            std::string name = node["Name"] && node["Name"].IsScalar() ? node["Name"].Scalar() : "GameObject";

            ObjectID parent = ReadID(node, "Parent");
            if (skipped.count(parent)) {
                skipped.insert(id);
                continue;
            }
            if (!id.IsValid() || byID.count(id) || scene.FindByID(id)) {
                VKE_CORE_WARN("SceneSerializer: '{}' in '{}' has a missing or repeated ID and is skipped", name,
                              sourceName);
                skipped.insert(id);
                continue;
            }

            YAML::Node prefabNode = node["Prefab"];
            if (prefabNode && prefabNode.IsScalar()) {
                GameObject* instance = nullptr;
                if (prefabs) {
                    instance = prefabs->Instantiate(scene, Assets::AssetID::Parse(prefabNode.Scalar()), nullptr, id);
                } else {
                    VKE_CORE_WARN("SceneSerializer: prefabs can't go inside prefabs; '{}' in '{}' is left empty", name,
                                  sourceName);
                }
                if (instance) {
                    instance->SetName(name);
                    std::vector<GameObject*> parts;
                    CollectPreOrder(*instance, parts);
                    for (GameObject* part : parts) {
                        byID[part->GetID()] = part;
                    }
                    loaded.push_back({node, instance, true});
                    continue;
                }
                if (prefabs) {
                    VKE_CORE_WARN("SceneSerializer: '{}' in '{}' uses a prefab that can't be loaded and is left empty",
                                  name, sourceName);
                }
            }

            GameObject& object = scene.CreateObject(name, id);
            byID[id] = &object;
            loaded.push_back({node, &object, false});
        }

        // Then the hierarchy, in file order so siblings keep their order.
        for (const Loaded& entry : loaded) {
            ObjectID parentID = ReadID(entry.Node, "Parent");
            if (!parentID.IsValid()) continue;
            auto parent = byID.find(parentID);
            if (parent == byID.end()) {
                VKE_CORE_WARN("SceneSerializer: '{}' has a parent that isn't in the file", entry.Object->GetName());
                continue;
            }
            entry.Object->SetParent(parent->second, false);
        }

        for (const Loaded& entry : loaded) {
            GameObject& object = *entry.Object;
            if (entry.IsInstance) {
                ReadOverrides(entry.Node["Overrides"], object, assets);
                continue;
            }
            ReadActive(entry.Node, object);

            YAML::Node components = entry.Node["Components"];
            if (!components || !components.IsMap()) continue;
            for (const auto& item : components) {
                std::string typeName = item.first.Scalar();
                const ComponentTypeInfo* type = &ComponentRegistry::FindOrAddMissing(typeName);
                Component* component = type->ID == ComponentRegistry::GetID<TransformComponent>()
                                           ? &object.GetTransform()
                                           : object.AddComponent(type->ID);
                if (!component) continue;
                if (type->Missing) {
                    VKE_CORE_WARN("SceneSerializer: '{}' has a component of unknown type '{}', kept as it is until "
                                  "the type is back",
                                  object.GetName(), typeName);
                    static_cast<MissingComponent*>(component)->SetSavedText(YAML::Dump(item.second));
                    continue;
                }

                PropertyReader reader(assets, object.GetName());
                reader.Read(*type->Properties, item.second, type->ToObject(component), std::string(type->Name) + ".");
                component->ApplyPropertyChanges(assets);
            }
        }

        // A scene that had no main camera takes the file's.
        if (!hadMainCamera) {
            if (auto it = byID.find(ReadID(root, "MainCamera")); it != byID.end()) {
                if (auto* camera = it->second->GetComponent<CameraComponent>()) scene.SetMainCamera(camera);
            }
        }

        VKE_CORE_INFO("SceneSerializer: loaded {} objects from '{}'", loaded.size(), sourceName);
        return true;
    }

} // namespace VKE::Scene
