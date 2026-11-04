#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/SlotMap.h>

#include <unordered_map>

namespace VKE {

// The assets of one type that AssetManager holds: their data by handle, and what each asset
// was loaded to, so loading it again returns the same handle. `T` has an `ID` member, the
// asset it was loaded from.
template <typename T> class AssetStore {
public:
	// Data added in code has no ID, whatever it says.
	Handle<T> Add(T data) {
		data.ID = {};
		return m_Data.Emplace(std::move(data));
	}

	// Records that asset `id` loads to `handle`, whose data takes the ID. Replaces what it
	// loaded to before.
	void SetLoaded(AssetID id, Handle<T> handle) {
		if (T *data = m_Data.Get(handle)) {
			data->ID = id;
			m_Loaded[id] = handle;
		}
	}

	// What asset `id` was loaded to, or invalid if it wasn't.
	Handle<T> FindLoaded(AssetID id) const {
		auto it = m_Loaded.find(id);
		return it != m_Loaded.end() && m_Data.Contains(it->second) ? it->second : Handle<T>{};
	}

	const std::unordered_map<AssetID, Handle<T>> &GetLoaded() const { return m_Loaded; }

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
	std::unordered_map<AssetID, Handle<T>> m_Loaded;
};

} // namespace VKE
