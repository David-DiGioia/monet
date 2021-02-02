#include "test_app.h"

#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"
#include "imgui.h"

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
