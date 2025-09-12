#pragma once
#include <vector>
#include "Component.h"
class Entity
{
public:
	Entity();
	~Entity();
private:
	std::vector<Component> components;
};

