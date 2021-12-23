#include <iostream>

#include "vk_engine.h"
#include "main_app.h"

int main(int argc, char* argv[])
{
#ifdef TRACY_ENABLE
	std::cout << "TRACY_ENABLE is defined\n";
#endif

	VulkanEngine engine;

	APP_NAME app{};

	engine.init(&app);
	
	engine.run();

	engine.cleanup();

	return 0;
}
