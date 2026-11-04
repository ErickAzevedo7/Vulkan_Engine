#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/SlotMap.h>

#include <functional>
#include <unordered_map>

namespace VKE {

// A texture is loaded once per color space it is read in.
struct TextureKey {
	AssetID ID;
	ColorSpace Space = ColorSpace::Linear;

	bool operator==(const TextureKey &other) const { return ID == other.ID && Space == other.Space; }
};

struct TextureKeyHash {
	size_t operator()(const TextureKey &key) const {
		return std::hash<AssetID>()(key.ID) ^ (key.Space == ColorSpace::SRGB ? 1 : 0);
	}
};

inline AssetID GetKeyID(AssetID id) {
	return id;
}

inline AssetID GetKeyID(const TextureKey &key) {
	return key.ID;
}

// The assets of one type that AssetManager holds: their data by handle, and what each asset
// was loaded to, so loading it again returns the same handle. `T` has an `ID` member, the
// asset it was loaded from.
template <typename T, typename Key = AssetID, typename Hash = std::hash<Key>> class AssetStore {
public:
	// Data added in code has no ID, whatever it says.
	Handle<T> Add(T data) {
		data.ID = {};
		return m_Data.Emplace(std::move(data));
	}

	// Records that `key` loads to `handle`, whose data takes the key's asset ID.
	void SetLoaded(const Key &key, Handle<T> handle) {
		if (T *data = m_Data.Get(handle)) {
			data->ID = GetKeyID(key);
			m_Loaded[key] = handle;
		}
	}

	// What `key` was loaded to, or invalid if it wasn't.
	Handle<T> FindLoaded(const Key &key) const {
		auto it = m_Loaded.find(key);
		return it != m_Loaded.end() && m_Data.Contains(it->second) ? it->second : Handle<T>{};
	}

	const std::unordered_map<Key, Handle<T>, Hash> &GetLoaded() const { return m_Loaded; }

	bool Contains(Handle<T> handle) const { return m_Data.Contains(handle); }
	T *Get(Handle<T> handle) { return m_Data.Get(handle); }
	const T *Get(Handle<T> handle) const { return m_Data.Get(handle); }

	// Invalid for data added in code.
	AssetID GetID(Handle<T> handle) const {
		const T *data = m_Data.Get(handle);
		return data ? data->ID : AssetID{};
	}

private:
	SlotMap<T> m_Data;
	std::unordered_map<Key, Handle<T>, Hash> m_Loaded;
};

} // namespace VKE
