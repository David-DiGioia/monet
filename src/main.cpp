#include <iostream>

#include "vk_engine.h"
#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"

GameObject sofa{};

void init(VulkanEngine& engine)
{
	GameObject bed{ engine.create_render_object("bed") };

	sofa.setRenderObject(engine.create_render_object("sofa"));
	sofa.setPos(glm::vec3(-2.5, 0.0, 0.4));
	sofa.setRot(glm::rotate(glm::radians(110.0f), glm::vec3{ 0.0, 1.0, 0.0 }));

	GameObject chair{ engine.create_render_object("chair") };
	chair.setPos(glm::vec3(-2.1, 0.0, -2.0));
	chair.setRot(glm::rotate(glm::radians(80.0f), glm::vec3{ 0.0, 1.0, 0.0 }));

	GameObject plane{ engine.create_render_object("plane", "default") };
}

void update(VulkanEngine& engine, float delta)
{
	glm::vec3 pos{ sofa.getPos() };
	pos.x += delta;
	sofa.setPos(pos);
}

int main(int argc, char* argv[])
{
#ifdef TRACY_ENABLE
	std::cout << "TRACY_ENABLE is defined\n";
#endif

	InitInfo info{};
	info.init = std::function<void(VulkanEngine&)>{ init };
	info.update = std::function<void(VulkanEngine&, float)>{ update };

	VulkanEngine engine;

	engine.init(info);
	
	engine.run();

	engine.cleanup();

	return 0;
}
