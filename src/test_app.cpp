#include "test_app.h"

#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"
#include "imgui.h"
#include "SDL.h"

#include <cmath>
#include <iostream>

void TestApp::init(VulkanEngine& engine)
{
	SDL_SetRelativeMouseMode((SDL_bool)_camMouseControls);

	_camera.pos = glm::vec3{ 0.0, 2.0, 2.0 };

	GameObject bed{ engine.create_render_object("bed") };

	_sofa.setRenderObject(engine.create_render_object("sofa"));
	_sofa.setPos(glm::vec3(-2.5, 0.0, 0.4));
	_sofa.setRot(glm::rotate(glm::radians(110.0f), glm::vec3{ 0.0, 1.0, 0.0 }));

	GameObject chair{ engine.create_render_object("chair") };
	chair.setPos(glm::vec3(-2.1, 0.0, -2.0));
	chair.setRot(glm::rotate(glm::radians(80.0f), glm::vec3{ 0.0, 1.0, 0.0 }));

	float halfExtent{ 1.0f };
	PxMaterial* material{ engine.create_physics_material(0.5, 0.5, 0.6) };
	PxShape* shape{ engine.create_physics_shape(PxBoxGeometry(halfExtent, halfExtent, halfExtent), *material) };

	_cube.setRenderObject(engine.create_render_object("cube", "default"));
	_cube.setPos(glm::vec3(0.0, 3.0, 0.0));
	engine.add_to_physics_engine(&_cube, shape);

	GameObject plane{ engine.create_render_object("plane", "default") };

	Light light{};
	light.color = glm::vec4{ 1.0, 0.1, 0.1, 100.0 };
	light.position = glm::vec4{ 1.0, 5.0, 1.0, 0.0 };

	_lights.push_back(light);
}

void TestApp::updateCamera(VulkanEngine& engine)
{
	glm::mat4 rotTheta{ glm::rotate(_camRotTheta, glm::vec3{ 1.0f, 0.0f, 0.0f }) };
	glm::mat4 rotPhi{ glm::rotate(_camRotPhi, glm::vec3{ 0.0f, 1.0f, 0.0f }) };
	_camera.rot = rotPhi * rotTheta;
	engine.set_camera_transform(_camera);
}

void TestApp::update(VulkanEngine& engine, float delta)
{
	glm::vec3 pos{ _sofa.getPos() };
	pos.x += delta;
	_sofa.setPos(pos);
	_lights[0].position.x = std::sinf(_time) * 3.0f;

	updateCamera(engine);
	engine.set_scene_lights(_lights);
	_time += delta;
}

void TestApp::fixedUpdate(VulkanEngine& engine)
{
	if (_applyForce) {
		_cube.getPhysicsObject()->addForce(physx::PxVec3{1000.0, 1000.0, 0.0});
	}
}

bool TestApp::input(float delta)
{
	float speed{ 3.0f };
	float camSensitivity{ 0.3f };
	// mouse motion seems to be sampled 60fps regardless of framerate
	constexpr float mouseDelta{ 1.0f / 60.0f };

	const Uint8* keystate{ SDL_GetKeyboardState(nullptr) };

	bool bQuit{ false };
	SDL_Event e;
	// Handle events on queue
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
		case SDL_KEYDOWN:
			if (e.key.keysym.sym == SDLK_f) {
				_camMouseControls = !_camMouseControls;
				SDL_SetRelativeMouseMode((SDL_bool)_camMouseControls);
			}
			break;
		case SDL_MOUSEMOTION:
			if (_camMouseControls) {
				_camRotPhi -= e.motion.xrel * camSensitivity * mouseDelta;
				_camRotTheta -= e.motion.yrel * camSensitivity * mouseDelta;
				_camRotTheta = std::clamp(_camRotTheta, -pi / 2.0f, pi / 2.0f);
			}
			break;
		case SDL_QUIT:
			bQuit = true;
			break;
		}
	}

	glm::vec4 translate{ 0.0f };

	// continuous-response keys
	if (keystate[SDL_SCANCODE_W]) {
		translate.z -= speed * delta;
	}
	if (keystate[SDL_SCANCODE_A]) {
		translate.x -= speed * delta;
	}
	if (keystate[SDL_SCANCODE_S]) {
		translate.z += speed * delta;
	}
	if (keystate[SDL_SCANCODE_D]) {
		translate.x += speed * delta;
	}
	if (keystate[SDL_SCANCODE_E]) {
		translate.y += speed * delta;
	}
	if (keystate[SDL_SCANCODE_Q]) {
		translate.y -= speed * delta;
	}

	if (keystate[SDL_SCANCODE_T]) {
		_applyForce = true;
	} else {
		_applyForce = false;
	}

	_camera.pos += glm::vec3{ _camera.rot * translate };

	return bQuit;
}

void TestApp::gui()
{
	ImGui::ShowDemoWindow();

	//// Main body of the Demo window starts here.
	//if (!ImGui::Begin("Debug"))
	//{
	//	// Early out if the window is collapsed, as an optimization.
	//	ImGui::End();
	//	return;
	//}

	//if (ImGui::CollapsingHeader("Lights"))
	//{
	//	ImGui::Text("Light 0:");
	//	ImGui::DragFloat3("position 0", (float*)&_guiData.light0.position, 0.005f);
	//	ImGui::ColorEdit3("color 0", (float*)&_guiData.light0.color);
	//	ImGui::DragFloat("intensity 0", &_guiData.light0.color.w, 0.02f);

	//	ImGui::Text("Light 1:");
	//	ImGui::DragFloat3("position 1", (float*)&_guiData.light1.position, 0.005f);
	//	ImGui::ColorEdit3("color 1", (float*)&_guiData.light1.color);
	//	ImGui::DragFloat("intensity 1", &_guiData.light1.color.w, 0.02f);
	//}

	//if (ImGui::CollapsingHeader("Camera"))
	//{
	//	ImGui::Text("Cam rotation:");
	//	ImGui::DragFloat("phi", &_camRotPhi, 0.005f);
	//	ImGui::DragFloat("theta", &_camRotTheta, 0.005f);
	//}

	//if (ImGui::CollapsingHeader("Bed"))
	//{
	//	ImGui::DragFloat("Bed angle", &_guiData.bedAngle, 0.005f);
	//}

	//ImGui::SliderFloat("Roughness", &_guiData.roughness_mult, 0.0f, 1.0f);

	//ImGui::End();
}
