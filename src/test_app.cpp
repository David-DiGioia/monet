#include "test_app.h"

#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"

void TestApp::init(VulkanEngine& engine)
{
	GameObject bed{ engine.create_render_object("bed") };

	_sofa.setRenderObject(engine.create_render_object("sofa"));
	_sofa.setPos(glm::vec3(-2.5, 0.0, 0.4));
	_sofa.setRot(glm::rotate(glm::radians(110.0f), glm::vec3{ 0.0, 1.0, 0.0 }));

	GameObject chair{ engine.create_render_object("chair") };
	chair.setPos(glm::vec3(-2.1, 0.0, -2.0));
	chair.setRot(glm::rotate(glm::radians(80.0f), glm::vec3{ 0.0, 1.0, 0.0 }));

	GameObject plane{ engine.create_render_object("plane", "default") };
}

void TestApp::update(VulkanEngine& engine, float delta)
{
	glm::vec3 pos{ _sofa.getPos() };
	pos.x += delta;
	_sofa.setPos(pos);
}

void TestApp::input()
{

}

void TestApp::gui()
{

}
