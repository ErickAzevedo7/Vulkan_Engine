#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

// Forward declaration
class Component;

class Entity {
public:
	bool isSelected = false;

	Entity(std::string name);

	~Entity();

	std::string getName() const;

	uint32_t getID() const {
		return id;
	}

	void setID(uint32_t newId) {
		id = newId;
	}

	static void updateNextID(uint32_t loadedId) {
		uint32_t current = nextID.load();
		while (loadedId >= current && !nextID.compare_exchange_weak(current, loadedId + 1));
	}

	template<typename T> T* getComponent() const {
		static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
		for (const auto& component : components) {
			if (T* casted = dynamic_cast<T*>(component)) {
				return casted;
			}
		}
		return nullptr;
	}

	template<typename T> bool hasComponent() const {
		return getComponent<T>() != nullptr;
	}

	template<typename T> std::vector<T*> getComponents() const {
		static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
		std::vector<T*> matches;
		for (const auto& component : components) {
			if (T* casted = dynamic_cast<T*>(component)) {
				matches.push_back(casted);
			}
		}
		return matches;
	}

	void addComponent(Component* component);
	bool removeComponent(Component* component);

	template<typename T> bool removeComponent() {
		static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
		T* component = getComponent<T>();
		if (!component) {
			return false;
		}
		return removeComponent(static_cast<Component*>(component));
	}

	void setName(char* str);

	Entity* getParent() const {
		return parent;
	}

	const std::vector<Entity*>& getChildren() const {
		return children;
	}

	bool hasParent() const {
		return parent != nullptr;
	}

	bool setParent(Entity* newParent, bool keepWorld = true);
	void clearParent(bool keepWorld = true);
	bool isDescendantOf(const Entity* potentialAncestor) const;

private:
	static std::atomic<uint32_t> nextID;
	std::vector<Component*> components;
	Entity* parent = nullptr;
	std::vector<Entity*> children;
	std::string name;
	uint32_t id;
};
