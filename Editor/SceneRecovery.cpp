#include "SceneRecovery.h"

#include <core/Log.h>

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace VKE::Editor {

bool SceneRecovery::Exists() const {
	std::error_code ec;
	return std::filesystem::exists(m_File, ec);
}

void SceneRecovery::Write(const std::string &text, AssetID scene) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Scene" << YAML::Value << YAML::DoubleQuoted << scene.ToString();
	out << YAML::Key << "Text" << YAML::Value << YAML::Literal << text;
	out << YAML::EndMap;
	std::ofstream stream(m_File, std::ios::binary | std::ios::trunc);
	stream << out.c_str() << '\n';
	if (!stream) {
		VKE_CORE_WARN("Editor: couldn't write '{}'", m_File.u8string());
	}
}

bool SceneRecovery::Read(std::string &text, AssetID &scene) const {
	YAML::Node root;
	try {
		root = YAML::LoadFile(m_File.string());
	} catch (const YAML::Exception &error) {
		VKE_CORE_ERROR("Editor: the recovered scene can't be read: {}", error.what());
		return false;
	}
	text = root["Text"] && root["Text"].IsScalar() ? root["Text"].Scalar() : std::string();
	scene = root["Scene"] && root["Scene"].IsScalar() ? AssetID::Parse(root["Scene"].Scalar()) : AssetID();
	return true;
}

void SceneRecovery::Discard() const {
	std::error_code ec;
	std::filesystem::remove(m_File, ec);
}

} // namespace VKE::Editor
