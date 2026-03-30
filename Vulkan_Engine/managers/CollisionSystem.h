#pragma once

#include <cstdint>
#include <unordered_set>

class Scene;
namespace Core { class EventBus; }

class CollisionSystem {
public:
	void reset();
	void update(Scene& scene, Core::EventBus* eventBus);

private:
	using PairKey = uint64_t;
	std::unordered_set<PairKey> activePairs;

	static PairKey makePairKey(uint32_t a, uint32_t b);
};
