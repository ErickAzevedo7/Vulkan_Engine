#pragma once

#include "scene/Properties.h"

#include <yaml-cpp/yaml.h>

#include <string>

namespace VKE {

class AssetManager;

// Writes the properties of `object` that differ from `defaults` as one YAML map. Loading
// starts from the same values, a new component's or a prefab's, so what's left out comes
// back as it was.
class PropertyWriter {
public:
	PropertyWriter(YAML::Emitter &out, const AssetManager &assets, const std::string &owner) :
			m_Out(out), m_Assets(assets), m_Owner(owner) {}

	void Write(const StructInfo &type, const void *object, const void *defaults);

	// For the operations of each property type.
	YAML::Emitter &Key(const PropertyInfo &property) { return m_Out << YAML::Key << property.Name << YAML::Value; }
	YAML::Emitter &GetEmitter() { return m_Out; }
	const AssetManager &GetAssets() const { return m_Assets; }
	void Warn(const PropertyInfo &property, const std::string &problem) const;

private:
	YAML::Emitter &m_Out;
	const AssetManager &m_Assets;
	const std::string &m_Owner;
};

// Sets the properties of `object` from one YAML map. Missing properties keep the value the
// object already has; malformed ones too, with a warning.
class PropertyReader {
public:
	PropertyReader(AssetManager &assets, const std::string &owner) : m_Assets(assets), m_Owner(owner) {}

	// `path` names the map in warnings, as "LightComponent.Settings.".
	void Read(const StructInfo &type, const YAML::Node &node, void *object, const std::string &path);

	// For the operations of each property type. False, with a warning, if the node doesn't hold
	// what's asked for.
	template <typename T> bool ReadScalar(const YAML::Node &node, T &value, const std::string &path) const {
		// Into a copy: a conversion that fails partway may leave a value behind.
		T read{};
		if (node.IsScalar() && YAML::convert<T>::decode(node, read)) {
			value = read;
			return true;
		}
		Warn(path, "has the wrong type");
		return false;
	}
	bool ReadNumbers(const YAML::Node &node, f32 *out, size_t count, const std::string &path) const;
	AssetManager &GetAssets() const { return m_Assets; }
	void Warn(const std::string &path, const std::string &problem) const;

private:
	AssetManager &m_Assets;
	const std::string &m_Owner;
};

// Everything a property type does, in one place: compare, copy, save and load. A new
// PropertyType gets its entry in PropertyOps.cpp.
struct PropertyOps {
	PropertyType Type;
	bool (*Equal)(const PropertyInfo &property, const void *a, const void *b);
	void (*Copy)(const PropertyInfo &property, void *to, const void *from);
	void (*Write)(PropertyWriter &out, const PropertyInfo &property, const void *value, const void *defaults);
	void (*Read)(PropertyReader &in, const PropertyInfo &property, const YAML::Node &node, void *value,
			const std::string &path);
};

const PropertyOps &GetPropertyOps(PropertyType type);

} // namespace VKE
