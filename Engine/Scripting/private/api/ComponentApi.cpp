#include "ApiCommon.h"

#include <assets/AssetManager.h>
#include <scene/Prefab.h>
#include <scene/components/CameraComponent.h>
#include <scene/components/LightComponent.h>
#include <scene/components/MeshComponent.h>

#include <vke/detail/ApiBindings.h>

#include <vector>

namespace vke::api {

    namespace {
        using VKE::Scene::CameraComponent;
        using VKE::Scene::LightComponent;
        using VKE::Scene::LightSettings;
        using VKE::Scene::MeshComponent;
        using VKE::Scripting::FindComponent;
        using VKE::Scripting::ScriptRun;

        // Changes a copy of the light's settings and hands it back, which syncs the light.
        template<typename Change>
        void ChangeLight(Light self, const Change& change) {
            LightComponent* light = FindComponent<LightComponent>(self.GetObject());
            if (!light) return;
            LightSettings settings = light->GetSettings();
            change(settings);
            light->SetSettings(settings);
        }
    } // namespace

    bool Light_IsValid(Light self) {
        return FindComponent<LightComponent>(self.GetObject()) != nullptr;
    }

    Vec3 Light_GetColor(Light self) {
        LightComponent* light = FindComponent<LightComponent>(self.GetObject());
        return light ? VKE::Scripting::ToSDK(light->GetSettings().Color) : Vec3();
    }

    void Light_SetColor(Light self, Vec3 color) {
        ChangeLight(self, [&](LightSettings& settings) { settings.Color = VKE::Scripting::ToEngine(color); });
    }

    float Light_GetIntensity(Light self) {
        LightComponent* light = FindComponent<LightComponent>(self.GetObject());
        return light ? light->GetSettings().Intensity : 0.0f;
    }

    void Light_SetIntensity(Light self, float intensity) {
        ChangeLight(self, [&](LightSettings& settings) { settings.Intensity = intensity; });
    }

    float Light_GetRange(Light self) {
        LightComponent* light = FindComponent<LightComponent>(self.GetObject());
        return light ? light->GetSettings().Range : 0.0f;
    }

    void Light_SetRange(Light self, float range) {
        ChangeLight(self, [&](LightSettings& settings) { settings.Range = range; });
    }

    bool Camera_IsValid(Camera self) {
        return FindComponent<CameraComponent>(self.GetObject()) != nullptr;
    }

    float Camera_GetFieldOfView(Camera self) {
        CameraComponent* camera = FindComponent<CameraComponent>(self.GetObject());
        return camera ? glm::degrees(camera->FieldOfView) : 0.0f;
    }

    void Camera_SetFieldOfView(Camera self, float degrees) {
        CameraComponent* camera = FindComponent<CameraComponent>(self.GetObject());
        if (camera) camera->FieldOfView = glm::radians(degrees);
    }

    void Camera_MakeMain(Camera self) {
        if (auto* camera = FindComponent<CameraComponent>(self.GetObject())) camera->GetScene().SetMainCamera(camera);
    }

    bool Mesh_IsValid(Mesh self) {
        return FindComponent<MeshComponent>(self.GetObject()) != nullptr;
    }

    void Mesh_SetMesh(Mesh self, MeshRef mesh) {
        MeshComponent* component = FindComponent<MeshComponent>(self.GetObject());
        if (!component) return;
        VKE::Assets::AssetID id{detail::Bits::GetID(mesh)};
        component->SetMesh(ScriptRun::GetCurrent()->GetPrefabs().GetAssets().LoadMesh(id));
    }

    void Mesh_SetMaterial(Mesh self, MaterialRef material, int slot) {
        MeshComponent* component = FindComponent<MeshComponent>(self.GetObject());
        if (!component || slot < 0) return;
        VKE::Assets::AssetID id{detail::Bits::GetID(material)};
        auto materials = component->GetMaterials();
        if (materials.size() <= static_cast<size_t>(slot)) materials.resize(static_cast<size_t>(slot) + 1);
        materials[static_cast<size_t>(slot)] = ScriptRun::GetCurrent()->GetPrefabs().GetAssets().LoadMaterial(id);
        component->SetMaterials(std::move(materials));
    }

} // namespace vke::api
