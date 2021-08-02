#pragma once
#include "vk_engine.h"
#include "application.h"

#include <vector>

class TestApp : public Application {
public:
	void init(VulkanEngine& engine) override;

	void update(VulkanEngine& engine, float delta) override;

	void fixedUpdate(VulkanEngine& engine) override;

	void input(const uint8_t* keystate, float delta) override;

	bool events(SDL_Event e) override;

	void gui(VulkanEngine& engine) override;

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
	GameObject _cube{};
	GameObject _cubeObj{};
	GameObject _skinning{};
	GameObject _chair{};

	// physics variables
	bool _applyForce{ false };

	glm::vec3 _skinningPos{ 0.0, 0.0, 0.0 };
	glm::vec3 _cubePos{ -2.5, 0.0, 0.4 };
	glm::vec3 _chairPos{ -2.1, 0.0, -2.0 };
};
