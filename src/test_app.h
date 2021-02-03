#pragma once
#include "vk_engine.h"
#include "application.h"

#include <vector>

class TestApp : public Application {
public:
	void init(VulkanEngine& engine) override;

	void update(VulkanEngine& engine, float delta) override;

	bool input(float delta) override;

	void gui() override;

	// New functions
	void updateCamera(VulkanEngine& engine);

private:
	// Camera variables
	Transform _camera{};
	float _camRotPhi{};
	float _camRotTheta{};
	bool _camMouseControls{ false };

	// cumulative time
	float _time{ 0.0f };

	// Lights
	std::vector<Light> _lights;

	// objects
	GameObject _sofa{};
};
