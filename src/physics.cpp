#include "physics.h"

#include <iostream>
#include "physics.h"

#include <ctype.h>

using namespace physx;

PxRigidDynamic* PhysicsEngine::createDynamic(const PxTransform& t, const PxGeometry& geometry, const PxVec3& velocity)
{
	PxRigidDynamic* dynamic = PxCreateDynamic(*_physics, t, geometry, *_material, 10.0f);
	dynamic->setAngularDamping(0.5f);
	dynamic->setLinearVelocity(velocity);
	_scene->addActor(*dynamic);
	return dynamic;
}

void PhysicsEngine::createStack(const PxTransform& t, PxU32 size, PxReal halfExtent)
{
	PxShape* shape = _physics->createShape(PxBoxGeometry(halfExtent, halfExtent, halfExtent), *_material);
	for (PxU32 i = 0; i < size; i++)
	{
		for (PxU32 j = 0; j < size - i; j++)
		{
			PxTransform localTm(PxVec3(PxReal(j * 2) - PxReal(size - i), PxReal(i * 2 + 1), 0) * halfExtent);
			PxRigidDynamic* body = _physics->createRigidDynamic(t.transform(localTm));
			body->attachShape(*shape);
			PxRigidBodyExt::updateMassAndInertia(*body, 10.0f);
			_scene->addActor(*body);
		}
	}
	shape->release();
}

void PhysicsEngine::initPhysics()
{
	_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, _allocator, _errorCallback);

	_pvd = PxCreatePvd(*_foundation);
	PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
	_pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

	_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *_foundation, PxTolerancesScale(), true, _pvd);

	PxSceneDesc sceneDesc(_physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	_dispatcher = PxDefaultCpuDispatcherCreate(2);
	sceneDesc.cpuDispatcher = _dispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;
	_scene = _physics->createScene(sceneDesc);

	PxPvdSceneClient* pvdClient = _scene->getScenePvdClient();
	if (pvdClient)
	{
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}
	_material = _physics->createMaterial(0.5f, 0.5f, 0.6f);

	//PxRigidStatic* groundPlane = PxCreatePlane(*_physics, PxPlane(0, 1, 0, 0), *_material);
	//_scene->addActor(*groundPlane);

	//for (PxU32 i = 0; i < 5; i++)
	//	createStack(PxTransform(PxVec3(0, 0, _stackZ -= 10.0f)), 10, 2.0f);

	//if (!interactive)
	//	createDynamic(PxTransform(PxVec3(0, 40, 100)), PxSphereGeometry(10), PxVec3(0, -50, -100));
}

void PhysicsEngine::stepPhysics(float stepSize)
{
	_scene->simulate(stepSize);
	// later this can be moved later so we don't block immediately after simulating
	_scene->fetchResults(true);
}

void PhysicsEngine::cleanupPhysics()
{
	PX_RELEASE(_scene);
	PX_RELEASE(_dispatcher);
	PX_RELEASE(_physics);
	if (_pvd) {
		PxPvdTransport* transport = _pvd->getTransport();
		_pvd->release();
		_pvd = nullptr;;
		PX_RELEASE(transport);
	}
	PX_RELEASE(_foundation);

	printf("PhysX objects cleaned up.\n");
}

PxRigidDynamic* PhysicsEngine::addToPhysicsEngineDynamic(const PxTransform& t, PxShape* shape, float density)
{
	PxTransform localTm(PxVec3(0.0f, 0.0f, 0));
	PxRigidDynamic* body{ _physics->createRigidDynamic(t.transform(localTm)) };
	body->attachShape(*shape);
	PxRigidBodyExt::updateMassAndInertia(*body, density);
	_scene->addActor(*body);
	return body;
}

PxRigidStatic* PhysicsEngine::addToPhysicsEngineStatic(const PxTransform& t, PxShape* shape)
{
	PxTransform localTm(PxVec3(0.0f, 0.0f, 0));
	PxRigidStatic* body{ _physics->createRigidStatic(t.transform(localTm)) };
	body->attachShape(*shape);
	_scene->addActor(*body);
	return body;
}

PxTransform PhysicsEngine::getActorTransform(PxRigidActor* body)
{
	return body->getGlobalPose();
}

PxMaterial* PhysicsEngine::createMaterial(float staticFriciton, float dynamicFriction, float restitution)
{
	return _physics->createMaterial(staticFriciton, dynamicFriction, restitution);
}

PxShape* PhysicsEngine::createShape(const PxGeometry& geometry, const PxMaterial& material, bool isExclusive, PxShapeFlags shapeFlags)
{
	return _physics->createShape(geometry, material, isExclusive, shapeFlags);
}

void PhysicsEngine::setGravity(float gravity)
{
	_scene->setGravity(PxVec3{ 0.0, gravity, 0.0 });
}

//void keyPress(unsigned char key, const PxTransform& camera)
//{
//	switch (toupper(key))
//	{
//	case 'B':	createStack(PxTransform(PxVec3(0, 0, stackZ -= 10.0f)), 10, 2.0f);						break;
//	case ' ':	createDynamic(camera, PxSphereGeometry(3.0f), camera.rotate(PxVec3(0, 0, -1)) * 200);	break;
//	}
//}
