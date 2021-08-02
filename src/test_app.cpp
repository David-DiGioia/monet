#include "test_app.h"

#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"
#include "imgui.h"

#include <cmath>
#include <iostream>

void TestApp::init(VulkanEngine& engine)
{
	SDL_SetRelativeMouseMode((SDL_bool)_camMouseControls);

	engine.setGravity(-9.81);

	_camera.pos = glm::vec3{ 0.0, 2.0, 2.0 };

	_skinning.setRenderObject(engine.createRenderObject("toad", "toad"));

	_cubeObj.setRenderObject(engine.createRenderObject("cube", "default"));

	//float halfExtent{ 1.0f };
	//PxMaterial* material{ engine.create_physics_material(0.5, 0.5, 0.6) };
	//PxShape* shape{ engine.create_physics_shape(PxBoxGeometry(halfExtent, halfExtent, halfExtent), *material) };

	//_cube.setRenderObject(engine.create_render_object("cube", "default"));
	//_cube.setPos(glm::vec3(0.0, 3.0, 0.0));
	//engine.add_to_physics_engine_dynamic(&_cube, shape);

	Light light{};
	light.color = glm::vec4{ 0.1, 1.0, 0.1, 0.0 };
	light.position = glm::vec4{ 1.0, 5.0, 1.0, 0.0 };

	_lights.push_back(light);
}

void TestApp::updateCamera(VulkanEngine& engine)
{
	glm::mat4 rotTheta{ glm::rotate(_camRotTheta, glm::vec3{ 1.0f, 0.0f, 0.0f }) };
	glm::mat4 rotPhi{ glm::rotate(_camRotPhi, glm::vec3{ 0.0f, 1.0f, 0.0f }) };
	_camera.rot = rotPhi * rotTheta;
	engine.setCameraTransform(_camera);
}

void TestApp::update(VulkanEngine& engine, float delta)
{
	_lights[0].position.x = std::sinf(_time) * 3.0f;
	_skinning.setPos(_skinningPos);
	_cubeObj.setPos(_cubePos);

	updateCamera(engine);
	engine.setSceneLights(_lights);
	_time += delta;
}

void TestApp::fixedUpdate(VulkanEngine& engine)
{

}

void TestApp::input(const uint8_t* keystate, float delta)
{
	float speed{ 3.0f };
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
}

bool TestApp::events(SDL_Event e)
{
	float camSensitivity{ 0.3f };
	// mouse motion seems to be sampled 60fps regardless of framerate
	constexpr float mouseDelta{ 1.0f / 60.0f };
	bool bQuit{ false };

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

	return bQuit;
}

void TestApp::gui(VulkanEngine& engine)
{
	//ImGui::ShowDemoWindow();

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

	ImGui::DragFloat3("Cube Pos", (float*)&_cubePos, 0.005f);
	ImGui::DragFloat3("Cylinder Pos", (float*)&_skinningPos, 0.005f);
	ImGui::SliderFloat("Roughness", &engine._guiData.roughness_mult, 0.0f, 1.0f);

	//ImGui::End();
}
