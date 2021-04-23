#pragma once

#include "SDL.h"

class VulkanEngine;

class Application {
public:
	virtual void init(VulkanEngine& engine) = 0;

	virtual void update(VulkanEngine& engine, float delta) = 0;

	virtual void fixedUpdate(VulkanEngine& engine) = 0;

	virtual void input(const uint8_t* keystate, float delta) = 0;

	virtual bool events(SDL_Event e) = 0;

	virtual void gui() = 0;
};
