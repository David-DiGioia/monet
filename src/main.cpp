#include <vk_engine.h>
#include <iostream>

int main(int argc, char* argv[])
{
#ifdef TRACY_ENABLE
	std::cout << "TRACY_ENABLE is defined\n";
#endif

	VulkanEngine engine;

	engine.init();
	
	engine.run();

	engine.cleanup();

	return 0;
}
