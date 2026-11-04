#include "PropertyOps.h"

#include <assets/AssetManager.h>
#include <core/Handle.h>
#include <core/Log.h>
#include <core/ObjectID.h>
#include <math/MathTypes.h>

#include <iterator>
#include <vector>

namespace VKE {

namespace {
using MaterialList = std::vector<Handle<MaterialData>>;

template <typename T> const T &As(const void *value) {
	return *static_cast<const T *>(value);
}

template <typename T> T &As(void *value) {
	return *static_cast<T *>(value);
}

// Values that == compares and = copies whole.
template <typename T> bool EqualValue(const PropertyInfo & /*property*/, const void *a, const void *b) {
	return As<T>(a) == As<T>(b);
}

template <typename T> void CopyValueOf(const PropertyInfo & /*property*/, void *to, const void *from) {
	As<T>(to) = As<T>(from);
}

// Bool, I32 and F32: one scalar.
template <typename T>
void WriteScalar(PropertyWriter &out, const PropertyInfo &property, const void *value, const void * /*defaults*/) {
	out.Key(property) << As<T>(value);
}

template <typename T>
void ReadScalar(PropertyReader &in, const PropertyInfo & /*property*/, const YAML::Node &node, void *value,
		const std::string &path) {
	in.ReadScalar(node, As<T>(value), path);
}

// Object and asset references: the ID in quotes. Nothing is an empty string, because the
// values compared with can have something.
template <typename ID>
void WriteID(PropertyWriter &out, const PropertyInfo &property, const void *value, const void * /*defaults*/) {
	const ID &id = As<ID>(value);
	out.Key(property) << YAML::DoubleQuoted << (id.IsValid() ? id.ToString() : std::string());
}

template <typename ID>
void ReadID(PropertyReader &in, const PropertyInfo & /*property*/, const YAML::Node &node, void *value,
		const std::string &path) {
	std::string text;
	if (in.ReadScalar(node, text, path)) {
		As<ID>(value) = ID::Parse(text);
	}
}

// Vec3: [x, y, z].
void WriteVec3(PropertyWriter &out, const PropertyInfo &property, const void *value, const void * /*defaults*/) {
	const Vec3 &v = As<Vec3>(value);
	out.Key(property) << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
}

void ReadVec3(PropertyReader &in, const PropertyInfo & /*property*/, const YAML::Node &node, void *value,
		const std::string &path) {
	f32 c[3];
	if (in.ReadNumbers(node, c, 3, path)) {
		As<Vec3>(value) = Vec3(c[0], c[1], c[2]);
	}
}

// Quat: [x, y, z, w], normalized when read.
void WriteQuat(PropertyWriter &out, const PropertyInfo &property, const void *value, const void * /*defaults*/) {
	const Quat &q = As<Quat>(value);
	out.Key(property) << YAML::Flow << YAML::BeginSeq << q.x << q.y << q.z << q.w << YAML::EndSeq;
}

void ReadQuat(PropertyReader &in, const PropertyInfo & /*property*/, const YAML::Node &node, void *value,
		const std::string &path) {
	f32 c[4];
	if (in.ReadNumbers(node, c, 4, path)) {
		As<Quat>(value) = glm::normalize(Quat(c[3], c[0], c[1], c[2]));
	}
}

// Enum: the value's name. Its size comes from PropertyInfo::Enum.
bool EqualEnum(const PropertyInfo &property, const void *a, const void *b) {
	return property.Enum->Read(a) == property.Enum->Read(b);
}

void CopyEnum(const PropertyInfo &property, void *to, const void *from) {
	property.Enum->Write(to, property.Enum->Read(from));
}

void WriteEnum(PropertyWriter &out, const PropertyInfo &property, const void *value, const void * /*defaults*/) {
	const EnumInfo &info = *property.Enum;
	u32 number = info.Read(value);
	for (const EnumValue *it = info.Values; it != info.Values + info.Count; ++it) {
		if (it->Value == number) {
			out.Key(property) << it->Name;
			return;
		}
	}
	out.Warn(property, "has a value " + std::string(info.Name) + " doesn't name, and isn't saved");
}

void ReadEnum(PropertyReader &in, const PropertyInfo &property, const YAML::Node &node, void *value,
		const std::string &path) {
	const EnumInfo &info = *property.Enum;
	std::string text;
	if (!in.ReadScalar(node, text, path)) {
		return;
	}
	for (const EnumValue *it = info.Values; it != info.Values + info.Count; ++it) {
		if (text == it->Name) {
			info.Write(value, it->Value);
			return;
		}
	}
	in.Warn(path, "'" + text + "' isn't a value of " + info.Name);
}

// Struct: a nested map, of what differs from the defaults inside it.
bool EqualStruct(const PropertyInfo &property, const void *a, const void *b) {
	return AreEqual(*property.Struct, a, b);
}

void CopyStruct(const PropertyInfo &property, void *to, const void *from) {
	CopyValues(*property.Struct, to, from);
}

void WriteStruct(PropertyWriter &out, const PropertyInfo &property, const void *value, const void *defaults) {
	out.Key(property);
	out.Write(*property.Struct, value, defaults);
}

void ReadStruct(PropertyReader &in, const PropertyInfo &property, const YAML::Node &node, void *value,
		const std::string &path) {
	in.Read(*property.Struct, node, value, path + ".");
}

// Mesh: the mesh's AssetID; nothing is an empty string. A mesh made in code isn't saved.
void WriteMesh(PropertyWriter &out, const PropertyInfo &property, const void *value, const void * /*defaults*/) {
	Handle<MeshData> mesh = As<Handle<MeshData>>(value);
	AssetID id = out.GetAssets().GetID(mesh);
	if (mesh.IsValid() && !id.IsValid()) {
		out.Warn(property, "uses an asset made in code, which isn't saved");
		return;
	}
	out.Key(property) << YAML::DoubleQuoted << (id.IsValid() ? id.ToString() : std::string());
}

void ReadMesh(PropertyReader &in, const PropertyInfo & /*property*/, const YAML::Node &node, void *value,
		const std::string &path) {
	std::string text;
	if (!in.ReadScalar(node, text, path)) {
		return;
	}
	auto &mesh = As<Handle<MeshData>>(value);
	mesh = {};
	if (text.empty()) {
		return;
	}
	mesh = in.GetAssets().LoadMesh(AssetID::Parse(text));
	if (!mesh.IsValid()) {
		in.Warn(path, "refers to a mesh that can't be loaded");
	}
}

// Materials: a list of AssetIDs. Materials made in code are written as empty strings, so the
// others keep their slots.
void WriteMaterials(PropertyWriter &out, const PropertyInfo &property, const void *value, const void * /*defaults*/) {
	out.Key(property) << YAML::Flow << YAML::BeginSeq;
	for (Handle<MaterialData> material : As<MaterialList>(value)) {
		AssetID id = out.GetAssets().GetID(material);
		if (material.IsValid() && !id.IsValid()) {
			out.Warn(property, "uses an asset made in code, which isn't saved");
		}
		out.GetEmitter() << YAML::DoubleQuoted << (id.IsValid() ? id.ToString() : std::string());
	}
	out.GetEmitter() << YAML::EndSeq;
}

void ReadMaterials(PropertyReader &in, const PropertyInfo & /*property*/, const YAML::Node &node, void *value,
		const std::string &path) {
	if (!node.IsSequence()) {
		in.Warn(path, "isn't a list");
		return;
	}
	MaterialList &materials = As<MaterialList>(value);
	materials.clear();
	for (const YAML::Node &item : node) {
		std::string text = item.IsScalar() ? item.Scalar() : std::string();
		Handle<MaterialData> material;
		if (!text.empty()) {
			material = in.GetAssets().LoadMaterial(AssetID::Parse(text));
			if (!material.IsValid()) {
				in.Warn(path, "refers to a material that can't be loaded");
			}
		}
		materials.push_back(material);
	}
}

// In the order of PropertyType, which GetPropertyOps indexes.
constexpr PropertyOps kOps[] = {
	{ PropertyType::Bool, EqualValue<bool>, CopyValueOf<bool>, WriteScalar<bool>, ReadScalar<bool> },
	{ PropertyType::I32, EqualValue<i32>, CopyValueOf<i32>, WriteScalar<i32>, ReadScalar<i32> },
	{ PropertyType::Object, EqualValue<ObjectID>, CopyValueOf<ObjectID>, WriteID<ObjectID>, ReadID<ObjectID> },
	{ PropertyType::Prefab, EqualValue<AssetID>, CopyValueOf<AssetID>, WriteID<AssetID>, ReadID<AssetID> },
	{ PropertyType::MeshAsset, EqualValue<AssetID>, CopyValueOf<AssetID>, WriteID<AssetID>, ReadID<AssetID> },
	{ PropertyType::MaterialAsset, EqualValue<AssetID>, CopyValueOf<AssetID>, WriteID<AssetID>, ReadID<AssetID> },
	{ PropertyType::SceneAsset, EqualValue<AssetID>, CopyValueOf<AssetID>, WriteID<AssetID>, ReadID<AssetID> },
	{ PropertyType::F32, EqualValue<f32>, CopyValueOf<f32>, WriteScalar<f32>, ReadScalar<f32> },
	{ PropertyType::Vec3, EqualValue<Vec3>, CopyValueOf<Vec3>, WriteVec3, ReadVec3 },
	{ PropertyType::Quat, EqualValue<Quat>, CopyValueOf<Quat>, WriteQuat, ReadQuat },
	{ PropertyType::Enum, EqualEnum, CopyEnum, WriteEnum, ReadEnum },
	{ PropertyType::Struct, EqualStruct, CopyStruct, WriteStruct, ReadStruct },
	{ PropertyType::Mesh, EqualValue<Handle<MeshData>>, CopyValueOf<Handle<MeshData>>, WriteMesh, ReadMesh },
	{ PropertyType::Materials, EqualValue<MaterialList>, CopyValueOf<MaterialList>, WriteMaterials, ReadMaterials },
};

constexpr bool IsInTypeOrder() {
	for (size_t i = 0; i < std::size(kOps); ++i) {
		if (static_cast<size_t>(kOps[i].Type) != i) {
			return false;
		}
	}
	return true;
}

static_assert(IsInTypeOrder(), "kOps must list the property types in the order PropertyType declares them");
static_assert(std::size(kOps) == static_cast<size_t>(PropertyType::Materials) + 1,
		"every PropertyType needs an entry in kOps");
} // namespace

// The static_asserts above keep every PropertyType inside the table.
const PropertyOps &GetPropertyOps(PropertyType type) {
	return kOps[static_cast<size_t>(type)];
}

bool AreEqual(const PropertyInfo &property, const void *a, const void *b) {
	return GetPropertyOps(property.Type).Equal(property, a, b);
}

bool AreEqual(const StructInfo &type, const void *a, const void *b) {
	for (const PropertyInfo &property : type) {
		if (!AreEqual(property, property.Get(a), property.Get(b))) {
			return false;
		}
	}
	return true;
}

void CopyValue(const PropertyInfo &property, void *to, const void *from) {
	GetPropertyOps(property.Type).Copy(property, to, from);
}

void CopyValues(const StructInfo &type, void *to, const void *from) {
	for (const PropertyInfo &property : type) {
		CopyValue(property, property.Get(to), property.Get(from));
	}
}

void PropertyWriter::Write(const StructInfo &type, const void *object, const void *defaults) {
	// Nothing to write fits on the key's line, as {}.
	if (AreEqual(type, object, defaults)) {
		m_Out << YAML::Flow << YAML::BeginMap << YAML::EndMap;
		return;
	}
	m_Out << YAML::BeginMap;
	for (const PropertyInfo &property : type) {
		const void *value = property.Get(object);
		if (AreEqual(property, value, property.Get(defaults))) {
			continue;
		}
		GetPropertyOps(property.Type).Write(*this, property, value, property.Get(defaults));
	}
	m_Out << YAML::EndMap;
}

void PropertyWriter::Warn(const PropertyInfo &property, const std::string &problem) const {
	VKE_CORE_WARN("SceneSerializer: {} on '{}' {}", property.Name, m_Owner, problem);
}

void PropertyReader::Read(const StructInfo &type, const YAML::Node &node, void *object, const std::string &path) {
	if (!node || node.IsNull()) {
		return;
	}
	if (!node.IsMap()) {
		Warn(path, "isn't a map of properties");
		return;
	}

	for (const PropertyInfo &property : type) {
		YAML::Node value = node[property.Name];
		if (!value) {
			continue;
		}
		GetPropertyOps(property.Type).Read(*this, property, value, property.Get(object), path + property.Name);
	}

	// A name the type doesn't have is usually a property that was renamed or removed.
	for (const auto &item : node) {
		std::string name = item.first.Scalar();
		bool known = false;
		for (const PropertyInfo &property : type) {
			known |= name == property.Name;
		}
		if (!known) {
			Warn(path + name, "isn't a property of " + std::string(type.Name) + " and is ignored");
		}
	}
}

bool PropertyReader::ReadNumbers(const YAML::Node &node, f32 *out, size_t count, const std::string &path) const {
	if (!node.IsSequence() || node.size() != count) {
		Warn(path, "needs " + std::to_string(count) + " numbers");
		return false;
	}
	try {
		for (size_t i = 0; i < count; ++i) {
			out[i] = node[i].as<f32>();
		}
		return true;
	} catch (const YAML::Exception &) {
		Warn(path, "has the wrong type");
		return false;
	}
}

void PropertyReader::Warn(const std::string &path, const std::string &problem) const {
	VKE_CORE_WARN("SceneSerializer: {} on '{}' {}", path, m_Owner, problem);
}

} // namespace VKE
