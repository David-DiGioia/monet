#pragma once
#include "vk_engine.h"
#include "application.h"

class TestApp : public Application {
public:
	void init(VulkanEngine& engine) override;

	void update(VulkanEngine& engine, float delta) override;

	void input() override;

	void gui() override;

private:
	GameObject _sofa{};
};
