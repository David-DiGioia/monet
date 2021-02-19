#pragma once

#include "PxPhysicsAPI.h"

#define PX_RELEASE(x)	if(x)	{ x->release(); x = NULL;	}
#define PVD_HOST "127.0.0.1"	//Set this to the IP address of the system running the PhysX Visual Debugger that you want to connect to.

using namespace physx;

class PhysicsEngine {
public:
	PxRigidDynamic* createDynamic(const PxTransform& t, const PxGeometry& geometry, const PxVec3& velocity = PxVec3(0));

	void createStack(const PxTransform& t, PxU32 size, PxReal halfExtent);

	void initPhysics();

	void stepPhysics(float stepSize);

	void cleanupPhysics();

	PxRigidDynamic* addToPhysicsEngineDynamic(const PxTransform& t, PxShape* shape, float density);

	PxRigidStatic* PhysicsEngine::addToPhysicsEngineStatic(const PxTransform& t, PxShape* shape);

	PxTransform getActorTransform(PxRigidActor* body);

	PxMaterial* createMaterial(float staticFriciton, float dynamicFriction, float restitution);

	PxShape* createShape(const PxGeometry& geometry,
		const PxMaterial& material,
		bool isExclusive = false,
		PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE);

	void setGravity(float gravity);

private:
	PxDefaultAllocator _allocator;
	PxDefaultErrorCallback _errorCallback;

	PxFoundation* _foundation{ nullptr };
	PxPhysics* _physics{ nullptr };

	PxDefaultCpuDispatcher* _dispatcher{ nullptr };
	PxScene* _scene{ nullptr };

	PxMaterial* _material{ nullptr };

	PxPvd* _pvd{ nullptr };

	PxReal _stackZ{ 10.0f };
};
