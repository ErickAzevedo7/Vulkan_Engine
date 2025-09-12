#pragma once
#include "Entity.h"
#include <vector>
class Scene
{
public:
	Scene();
	~Scene();

private:
	std::vector<Entity> entities;
};


