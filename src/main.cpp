#include <iostream>

#include "vk_engine.h"

void init(VulkanEngine& engine)
{

}

void update(VulkanEngine& engine, float delta)
{

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

	engine.init();
	
	engine.run();

	engine.cleanup();

	return 0;
}
