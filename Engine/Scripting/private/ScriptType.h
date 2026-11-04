#pragma once

#include <scene/ComponentRegistry.h>
#include <scene/Properties.h>
#include <vke/detail/GameInfo.h>

#include <deque>
#include <string>
#include <vector>

namespace VKE {

// A script type of the game's DLL, in the terms the engine works with. The tables keep
// pointing into the DLL, so this doesn't outlive it.
struct ScriptType {
	std::string Name;
	ComponentTypeID ID = kInvalidComponentType;
	void *(*Create)() = nullptr;
	void (*Destroy)(void *script) = nullptr;
	vke::Script *(*ToScript)(void *script) = nullptr;
	size_t Size = 0;
	// The registry's default-constructed instance, which an instance that failed copies.
	void *DefaultInstance = nullptr;
	const vke::detail::ScriptCalls *Calls = nullptr;
	bool UsesUpdate = false;
	bool UsesLateUpdate = false;
	StructInfo Properties = { nullptr, nullptr, 0 };

	// What Properties points at.
	std::deque<std::vector<PropertyInfo>> PropertyLists;
	std::deque<std::vector<EnumValue>> EnumValues;
	std::deque<EnumInfo> Enums;
	std::deque<StructInfo> Structs;
};

} // namespace VKE
