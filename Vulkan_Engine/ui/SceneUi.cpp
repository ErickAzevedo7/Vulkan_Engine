#include "SceneUi.h"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include "context/ResourceContext.h"
#include "Entity.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "managers/PrefabSerializer.h"
#include "managers/SceneManager.h"
#include "Scene.h"
#include "ui/InspectorUi.h"

namespace {

std::filesystem::path resolveProjectsRoot() {
#ifdef _WIN32
	char exeBuf[MAX_PATH] = {};
	GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
	std::filesystem::path projectsDir = std::filesystem::path(exeBuf).parent_path() / ".." / ".." / "projects";
	std::error_code ec;
	return std::filesystem::weakly_canonical(projectsDir, ec);
#else
	return std::filesystem::path("projects");
#endif
}

bool drawEntityTreeNode(Entity& entity, Scene& scene, InspectorUi& inspector, bool& droppedOnEntityTarget) {
	const int selectedId = inspector.getSelectedEntityId();
	const bool isSelected = (selectedId == static_cast<int>(entity.getID()));

	ImGuiTreeNodeFlags flags =
		ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (entity.getChildren().empty()) {
		flags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (isSelected) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	std::string label = entity.getName().empty() ? ("Entity " + std::to_string(entity.getID())) : entity.getName();
	const bool opened =
		ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(entity.getID())), flags, "%s", label.c_str());

	if (ImGui::IsItemClicked()) {
		inspector.selectEntity(static_cast<int>(entity.getID()));
	}

	if (ImGui::BeginDragDropSource()) {
		const uint32_t sourceId = entity.getID();
		ImGui::SetDragDropPayload("SCENE_ENTITY_ID", &sourceId, sizeof(sourceId));
		ImGui::TextUnformatted(label.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_ENTITY_ID")) {
			if (payload->Data && payload->DataSize == sizeof(uint32_t)) {
				const uint32_t sourceId = *static_cast<const uint32_t*>(payload->Data);
				Entity* sourceEntity = scene.findEntityById(sourceId);
				if (sourceEntity && sourceEntity != &entity && payload->IsDelivery()) {
					droppedOnEntityTarget = true;
					if (sourceEntity->setParent(&entity, true)) {
						scene.markDirty();
					}
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	bool removed = false;
	if (ImGui::BeginPopupContextItem()) {
		if (ImGui::MenuItem("Unparent", nullptr, false, entity.hasParent())) {
			entity.clearParent(true);
			scene.markDirty();
		}
		if (ImGui::MenuItem("Create Prefab")) {
			namespace fs = std::filesystem;
			fs::path prefabDir = resolveProjectsRoot() / "prefabs";
			std::error_code ec;
			fs::create_directories(prefabDir, ec);
			std::string baseName = entity.getName().empty() ? "Entity" : entity.getName();
			for (char& c : baseName) {
				if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
					c = '_';
				}
			}
			if (baseName.empty()) {
				baseName = "Entity";
			}

			fs::path prefabPath = prefabDir / (baseName + ".prefab");
			int suffix = 1;
			while (fs::exists(prefabPath)) {
				prefabPath = prefabDir / (baseName + "_" + std::to_string(suffix++) + ".prefab");
			}

			PrefabSerializer::save(prefabPath.string(), entity);
		}
		if (ImGui::MenuItem("Remove")) {
			scene.removeEntityById(entity.getID());
			if (inspector.getSelectedEntityId() == static_cast<int>(entity.getID())) {
				inspector.clearSelection();
			}
			removed = true;
		}
		ImGui::EndPopup();
	}

	if (removed) {
		if (opened) {
			ImGui::TreePop();
		}
		return true;
	}

	if (opened) {
		for (Entity* child : entity.getChildren()) {
			if (!child) {
				continue;
			}

			if (drawEntityTreeNode(*child, scene, inspector, droppedOnEntityTarget)) {
				break;
			}
		}
		ImGui::TreePop();
	}

	return false;
}

} // namespace

SceneUi::SceneUi(ResourceContext& resources, InspectorUi& inspector) : resources(resources), inspector(inspector) {
	selectedEntity = -1;
}

void SceneUi::render() {
	Scene* scene = resources.getSceneManager().getActiveScene();

	ImGui::Begin("scene");
	if (scene) {
		auto* entities = scene->getEntities();
		if (entities) {
			bool droppedOnEntityTarget = false;
			const ImVec2 originalFramePadding = ImGui::GetStyle().FramePadding;
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, originalFramePadding.y));

			const float treeStartOffset = ImGui::GetStyle().FramePadding.x;
			ImGui::Unindent(treeStartOffset);

			for (const auto& entityPtr : *entities) {
				if (!entityPtr || entityPtr->hasParent()) {
					continue;
				}

				if (drawEntityTreeNode(*entityPtr, *scene, inspector, droppedOnEntityTarget)) {
					break;
				}
			}

			ImGui::Indent(treeStartOffset);
			ImGui::PopStyleVar();

			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window && ImGui::BeginDragDropTargetCustom(window->InnerRect, window->ID)) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
						"ASSET_BROWSER_FILE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
					std::string filePath(static_cast<const char*>(payload->Data), payload->DataSize - 1);
					std::filesystem::path p(filePath);
					std::string ext = p.extension().string();
					for (char& c : ext) {
						c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
					}

					if (ext == ".prefab" && payload->IsDelivery()) {
						Entity* spawned = PrefabSerializer::instantiate(filePath, scene, resources);
						if (spawned) {
							inspector.selectEntity(static_cast<int>(spawned->getID()));
						}
					}
				}

				if (const ImGuiPayload* payload =
						ImGui::AcceptDragDropPayload("SCENE_ENTITY_ID", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
					if (payload->Data && payload->DataSize == sizeof(uint32_t) && payload->IsDelivery() &&
						!droppedOnEntityTarget) {
						const uint32_t sourceId = *static_cast<const uint32_t*>(payload->Data);
						Entity* sourceEntity = scene->findEntityById(sourceId);
						if (sourceEntity && sourceEntity->hasParent()) {
							sourceEntity->clearParent(true);
							scene->markDirty();
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
		}
	} else {
		ImGui::Text("No active scene.");
	}
	ImGui::End();
}
