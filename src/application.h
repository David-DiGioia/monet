#pragma once

class VulkanEngine;

class Application {
public:
	virtual void init(VulkanEngine& engine) = 0;

	virtual void update(VulkanEngine& engine, float delta) = 0;

	virtual void input() = 0;

	virtual void gui() = 0;
};
