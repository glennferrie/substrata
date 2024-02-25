/*=====================================================================
LoadModelTask.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../shared/WorldObject.h"
#include "../shared/Avatar.h"
#include "PhysicsObject.h"
#include <opengl/OpenGLEngine.h>
#include <Task.h>
#include <ThreadMessage.h>
#include <ThreadSafeQueue.h>
#include <string>
class OpenGLEngine;
class MeshManager;
class ResourceManager;


class ModelLoadedThreadMessage : public ThreadMessage
{
public:
	ModelLoadedThreadMessage() = default;
	GLARE_DISABLE_COPY(ModelLoadedThreadMessage);
	// Results of the task:
	
	Reference<OpenGLMeshRenderData> gl_meshdata;
	PhysicsShape physics_shape;
	
	std::string lod_model_url; // URL of the model we loaded.  Empty when loaded voxel object.
	bool built_dynamic_physics_ob;

	UID voxel_ob_uid; // Valid if we are loading voxel for an object, invalid otherwise.  Avoid storing WorldObjectRef to avoid dangling refs
	int voxel_ob_model_lod_level;// If we loaded a voxel model, the model LOD level of the object.
	int subsample_factor; // Computed when loading voxels.
};


/*=====================================================================
LoadModelTask
-------------
For the WorldObject ob, 
Builds the OpenGL mesh and Physics mesh for it.

Once it's done, sends a ModelLoadedThreadMessage back to the main window
via result_msg_queue.

Note for making the OpenGL Mesh, data isn't actually loaded into OpenGL in this task,
since that needs to be done on the main thread.
=====================================================================*/
class LoadModelTask : public glare::Task
{
public:
	LoadModelTask();
	virtual ~LoadModelTask();

	virtual void run(size_t thread_index);

	std::string lod_model_url; // The URL of a model with a specific LOD level to load.  Empty when loading voxel object.
	bool build_dynamic_physics_ob; // If true, build a convex hull shape instead of a mesh physics shape.
	
	WorldObjectRef voxel_ob; // If non-null, the task is to load/mesh the voxels for this object.
	int voxel_ob_model_lod_level; // If we are loading a voxel model, the model LOD level of the object.

	PhysicsShape unit_cube_shape;
	Reference<OpenGLEngine> opengl_engine;
	Reference<ResourceManager> resource_manager;
	ThreadSafeQueue<Reference<ThreadMessage> >* result_msg_queue;
};
