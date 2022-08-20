/*=====================================================================
PlayerPhysics.cpp
-----------------
Copyright Glare Technologies Limited 2021 -
File created by ClassTemplate on Mon Sep 23 15:14:04 2002
=====================================================================*/
#include "PlayerPhysics.h"


#include "CameraController.h"
#include "PhysicsWorld.h"
#include "PhysicsObject.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"


static const float runfactor = 5; // How much faster you move when the run button (shift) is held down.
static const float movespeed = 3;
static const float jumpspeed = 4.5;
static const float maxairspeed = 8;


static const float JUMP_PERIOD = 0.1f; // Allow a jump command to be executed even if the player is not quite on the ground yet.

const float SPHERE_RAD = 0.3f;
const float EYE_HEIGHT = 1.67f;

PlayerPhysics::PlayerPhysics()
:	vel(0,0,0),
	moveimpulse(0,0,0),
	lastgroundnormal(0,0,1),
	lastvel(0,0,0),
	jumptimeremaining(0),
	onground(false),
	flymode(false),
	last_runpressed(false),
	time_since_on_ground(0),
	campos_z_delta(0)
{
}


PlayerPhysics::~PlayerPhysics()
{
}


inline float doRunFactor(bool runpressed)
{
	if(runpressed)
		return runfactor;
	else
		return 1.0f;
}


void PlayerPhysics::processMoveForwards(float factor, bool runpressed, CameraController& cam)
{
	last_runpressed = runpressed;
	moveimpulse += ::toVec3f(cam.getForwardsVec()) * factor * movespeed * doRunFactor(runpressed);
}


void PlayerPhysics::processStrafeRight(float factor, bool runpressed, CameraController& cam)
{
	last_runpressed = runpressed;
	moveimpulse += ::toVec3f(cam.getRightVec()) * factor * movespeed * doRunFactor(runpressed);

}


void PlayerPhysics::processMoveUp(float factor, bool runpressed, CameraController& cam)
{
	last_runpressed = runpressed;
	if(flymode)
		moveimpulse += Vec3f(0,0,1) * factor * movespeed * doRunFactor(runpressed);
}


void PlayerPhysics::processJump(CameraController& cam)
{
	jumptimeremaining = JUMP_PERIOD;
}


void PlayerPhysics::setFlyModeEnabled(bool enabled)
{
	flymode = enabled;
}


static const Vec3f doSpringRelaxation(const std::vector<SpringSphereSet>& springspheresets,
										bool constrain_to_vertical, bool do_fast_mode);


UpdateEvents PlayerPhysics::update(PhysicsWorld& physics_world, float raw_dtime, Vec4f& campos_in_out)
{
	//printVar(onground);
	//conPrint("lastgroundnormal: " + lastgroundnormal.toString());

	const float dtime = myMin(raw_dtime, 0.1f); // Put a cap on dtime, so that if there is a long pause between update() calls for some reason (e.g. loading objects), 
	// then the user doesn't fly off into space or similar.

	UpdateEvents events;
	

	//-----------------------------------------------------------------
	//apply any jump impulse
	//-----------------------------------------------------------------
	if(jumptimeremaining > 0)
	{
		if(onground)
		{
			onground = false;
			vel += Vec3f(0,0,1) * jumpspeed;
			events.jumped = true;

			time_since_on_ground = 1; // Hack this up to a large value so jump animation can play immediately.
		}
	}

	jumptimeremaining -= dtime;
		
	//-----------------------------------------------------------------
	//apply movement forces
	//-----------------------------------------------------------------
	if(!flymode) // if not flying
	{
		if(onground)
		{
			//-----------------------------------------------------------------
			//restrict movement to parallel to plane standing on,
			//otherwise will 'take off' from downwards sloping surfaces when walking.
			//-----------------------------------------------------------------
			Vec3f parralel_impulse = moveimpulse;
			parralel_impulse.removeComponentInDir(lastgroundnormal);
			//dvel += parralel_impulse;

			vel = parralel_impulse;

			//------------------------------------------------------------------------
			//add the velocity of the object we are standing on
			//------------------------------------------------------------------------
			//if(lastgroundagent)
			//{
			//	vel += lastgroundagent->getVelocity(toVec3f(campos_out));
			//}
		}

		//-----------------------------------------------------------------
		//apply gravity
		//-----------------------------------------------------------------
		Vec3f dvel(0, 0, -9.81f);

		if(!onground)
		{
			//-----------------------------------------------------------------
			//restrict move impulse to horizontal plane
			//-----------------------------------------------------------------
			Vec3f horizontal_impulse = moveimpulse;
			horizontal_impulse.z = 0;
			//horizontal_impulse.removeComponentInDir(lastgroundnormal);
			dvel += horizontal_impulse;

			//-----------------------------------------------------------------
			//restrict move impulse to length maxairspeed ms^-2
			//-----------------------------------------------------------------
			Vec2f horiz_vel(dvel.x, dvel.y);
			if(horiz_vel.length() > maxairspeed)
				horiz_vel.setLength(maxairspeed);

			dvel.x = horiz_vel.x;
			dvel.y = horiz_vel.y;
		}

		vel += dvel * dtime;

		if(vel.z < -100) // cap falling speed at 100 m/s
			vel.z = -100;
	}
	else
	{
		// Desired velocity is maintaining the current speed but pointing in the moveimpulse direction.
		const float speed = vel.length();
		const Vec3f desired_vel = (moveimpulse.length() < 1.e-4f) ? Vec3f(0.f) : (normalise(moveimpulse) * speed);

		const Vec3f accel = moveimpulse * 3.f
		 + (desired_vel - vel) * 2.f;

		vel += accel * dtime;
	}

	//if(onground)
	//	debugPrint("onground."); 
	
	onground = false;
	//lastgroundagent = NULL;
	
	//-----------------------------------------------------------------
	//'integrate' to find new pos (Euler integration)
	//-----------------------------------------------------------------
	Vec3f dpos = vel*dtime;

	Vec3f campos = toVec3f(campos_in_out) + Vec3f(0, 0, campos_z_delta); // Physics/actual campos is below camera campos.

	//campos_z_delta = myMax(0.f, campos_z_delta - 2.f * dtime); // Linearly reduce campos_z_delta over time until it reaches 0.
	campos_z_delta = myMax(0.f, campos_z_delta - 20.f * dtime * campos_z_delta); // Exponentially reduce campos_z_delta over time until it reaches 0.

	for(int i=0; i<5; ++i)
	{	
		if(dpos != Vec3f(0,0,0))
		{		
			//conPrint("-----dpos: " + dpos.toString() + "----");
			//conPrint("iter: " + toString(i));

			//-----------------------------------------------------------------
			//do a trace along desired movement path to see if obstructed
			//-----------------------------------------------------------------
			float closest_dist = 1e9f;
			Vec4f closest_hit_pos_ws(0.f);
			Vec3f hit_normal;
			bool closest_point_in_tri = false;
			bool hitsomething = false;

			for(int s=0; s<3; ++s)//for each sphere in body
			{
				//-----------------------------------------------------------------
				//calc initial sphere position
				//-----------------------------------------------------------------
				// NOTE: The order of these spheres actually makes a difference, even though it shouldn't.
				// When s=0 is the bottom sphere, hit_normal may end up as (0,0,1) from a distance=0 hit, which means on_ground is set and spring relaxation is constrained to z-dir, which results in getting stuck.
				// So instead may sphere 0 the top sphere.
				// NEW: Actually removing the constrain-to-vertical constraint in sphere relaxation makes this not necessary.
				//const Vec3f spherepos = Vec3f(campos.x, campos.y, campos.z - EYE_HEIGHT + SPHERE_RAD * (1 + 2 * s));
				const Vec3f spherepos = Vec3f(campos.x, campos.y, campos.z - EYE_HEIGHT + SPHERE_RAD * (5 - 2 * s));

				const js::BoundingSphere playersphere(spherepos.toVec4fPoint(), SPHERE_RAD);

				//-----------------------------------------------------------------
				//trace sphere through world
				//-----------------------------------------------------------------
				SphereTraceResult traceresults;
				physics_world.traceSphere(playersphere, dpos.toVec4fVector(), traceresults);

				if(traceresults.hit_object)
				{
					//assert(traceresults.fraction >= 0 && traceresults.fraction <= 1);

					const float distgot = traceresults.hitdist_ws;
					//printVar(distgot);

					if(distgot < closest_dist)
					{
						hitsomething = true;
						closest_dist = distgot;
						closest_hit_pos_ws = traceresults.hit_pos_ws;
						hit_normal = toVec3f(traceresults.hit_normal_ws);
						closest_point_in_tri = traceresults.point_in_tri;
						assert(hit_normal.isUnitLength());

						//void* userdata = traceresults.hit_object->getUserdata();

						/*if(userdata)
						{
							Agent* agenthit = static_cast<Agent*>(userdata);
							lastgroundagent = agenthit;
						}*/
					}
				}
			}

			if(hitsomething) // if any of the spheres hit something:
			{
				//const float movedist = closest_dist;//max(closest_dist - 0.0, 0);
				//const float usefraction = movedist / dpos.length();
				const float usefraction = closest_dist / dpos.length();
				assert(usefraction >= 0 && usefraction <= 1);

				//debugPrint("traceresults.fraction: " + toString(traceresults.fraction));

		
				campos += dpos * usefraction; // advance camera position
				
				dpos *= (1.0f - usefraction); // reduce remaining distance by dist moved cam.

				//---------------------------------- Do stair climbing ----------------------------------
				// This is done by detecting if we have hit the edge of a step.
				// A step hit is categorised as any hit that is within a certain distance above the ground/foot level.
				// If we do hit a step, we displace the player upwards to just above the step, so it can continue its movement forwards over the step without obstruction.
				// Work out if we hit the edge of a step
				const float foot_z = campos[2] - EYE_HEIGHT;
				const float hitpos_height_above_foot = closest_hit_pos_ws[2] - foot_z;

				//bool hit_step = false;
				if(!closest_point_in_tri && hitpos_height_above_foot > 0.003f && hitpos_height_above_foot < 0.25f)
				{
					//hit_step = true;

					const float jump_up_amount = hitpos_height_above_foot + 0.01f; // Distance to displace the player upwards

					// conPrint("hit step (hitpos_height_above_foot: " + doubleToStringNSigFigs(hitpos_height_above_foot, 4) + "), jump_up_amount: " + doubleToStringNSigFigs(jump_up_amount, 4));

					// Trace a sphere up to see if we can raise up the avatar over the step without obstruction (we don't want to displace the head upwards into an overhanging object)
					const Vec3f spherepos = Vec3f(campos.x, campos.y, campos.z - EYE_HEIGHT + SPHERE_RAD * 5); // Upper sphere centre
					const js::BoundingSphere playersphere(spherepos.toVec4fPoint(), SPHERE_RAD);
					SphereTraceResult traceresults;
					physics_world.traceSphere(playersphere, /*translation_ws=*/Vec4f(0, 0, jump_up_amount, 0), traceresults); // Trace sphere through world

					if(!traceresults.hit_object)
					{
						campos.z += jump_up_amount;
						campos_z_delta = myMin(0.3f, campos_z_delta + jump_up_amount);

						hit_normal = Vec3f(0,0,1); // the step edge normal will be oriented towards the swept sphere centre at the collision point.
						// However consider it pointing straight up, so that next think the player is considered to be on flat ground and hence moves in the x-y plane.
					}
					else
					{
						conPrint("hit an object while tracing sphere up for jump");
					}
				}
				//---------------------------------- End stair climbing ----------------------------------


				const bool was_just_falling = vel.x == 0 && vel.y == 0;

				//-----------------------------------------------------------------
				//kill remaining translation normal to obstructor
				//-----------------------------------------------------------------
				dpos.removeComponentInDir(hit_normal);

				//-----------------------------------------------------------------
				//kill velocity in direction of obstructor normal
				//-----------------------------------------------------------------
				//lastvel = vel;
				vel.removeComponentInDir(hit_normal);

				//-----------------------------------------------------------------
				//if this is an upwards sloping surface, consider it ground.
				//-----------------------------------------------------------------
				if(hit_normal.z > 0.5)
				{
					onground = true;

					//-----------------------------------------------------------------
					//kill all remaining velocity and movement delta, cause the player is
					//now standing on something
					//-----------------------------------------------------------------
					//dpos.set(0,0,0);
					//vel.set(0,0,0);

					lastgroundnormal = hit_normal;

					//-----------------------------------------------------------------
					//kill remaining dpos to prevent sliding down slopes
					//-----------------------------------------------------------------
					if(was_just_falling)
						dpos.set(0,0,0);
				}

				// conPrint("Sphere trace hit something.   hit_normal: " + hit_normal.toString() + ", onground: " + boolToString(onground)); 
			}
			else
			{
				//didn't hit something, so finish all movement.
				campos += dpos;
				dpos.set(0,0,0);
			}	

			//-----------------------------------------------------------------
			//make sure sphere is outside of any object as much as possible
			//-----------------------------------------------------------------
			springspheresets.resize(3);
			for(int s=0; s<3; ++s)//for each sphere in body
			{
				//-----------------------------------------------------------------
				//calc position of sphere
				//-----------------------------------------------------------------
				const Vec3f spherepos = campos - Vec3f(0,0, (EYE_HEIGHT - 1.5f) + (float)s * 0.6f);

				const float REPEL_RADIUS = SPHERE_RAD + 0.005f;//displace to just off the surface
				js::BoundingSphere bigsphere(spherepos.toVec4fPoint(), REPEL_RADIUS);

				springspheresets[s].sphere = bigsphere;
				//-----------------------------------------------------------------
				//get the collision points
				//-----------------------------------------------------------------
				physics_world.getCollPoints(bigsphere, springspheresets[s].collpoints);
				
			}

			// Do a fast pass of spring relaxation.  This is basically just to determine if we are standing on a ground surface.
			Vec3f displacement = doSpringRelaxation(springspheresets, /*constrain to vertical=*/false, /*do_fast_mode=*/true);

			// If we were repelled from an upwards facing surface, consider us to be on the ground.
			if(displacement != Vec3f(0, 0, 0) && normalise(displacement).z > 0.5f)
				onground = true;

			// If we are standing on a ground surface, and not trying to move (moveimpulse = 0), then constrain to vertical movement.
			// This prevents sliding down ramps due to relaxation pushing in the normal direction.
			displacement = doSpringRelaxation(springspheresets, /*constrain to vertical=*/onground && (moveimpulse == Vec3f(0.f)), /*do_fast_mode=*/false);
			campos += displacement;
		}
	}

	if(!onground)
		time_since_on_ground += dtime;
	else
		time_since_on_ground = 0;
		
	campos_in_out = (campos - Vec3f(0,0,campos_z_delta)).toVec4fPoint();

	moveimpulse.set(0,0,0);

	return events;
}


static const Vec3f doSpringRelaxation(const std::vector<SpringSphereSet>& springspheresets,
										bool constrain_to_vertical, bool do_fast_mode)
{
	Vec3f displacement(0,0,0); // total displacement so far of spheres

	int num_iters_done = 0;
	const int max_num_iters = do_fast_mode ? 1 : 100;
	for(int i=0; i<max_num_iters; ++i)
	{	
		num_iters_done++;
		Vec3f force(0,0,0); // sum of forces acting on spheres from all springs
		int numforces = 0; // num forces acting on spheres

		for(size_t s=0; s<springspheresets.size(); ++s)
		{
			const Vec3f currentspherepos = toVec3f(springspheresets[s].sphere.getCenter()) + displacement;

			for(size_t c=0; c<springspheresets[s].collpoints.size(); ++c)
			{
				//-----------------------------------------------------------------
				//get vec from collision point to sphere center == spring vec
				//-----------------------------------------------------------------
				Vec3f springvec = currentspherepos - toVec3f(springspheresets[s].collpoints[c]);
				const float springlen = springvec.normalise_ret_length();

				//::debugPrint("springlen: " + toString(springlen));

				//const float excesslen = springlen - repel_radius;

				//if coll point is inside sphere...
				if(springlen < springspheresets[s].sphere.getRadius())
				{
					//force = springvec * dist coll point is inside sphere
					force += springvec * (springspheresets[s].sphere.getRadius() - springlen);
					++numforces;
				}


				//if(excesslen < 0)
				//{
				//	force += springvec * excesslen * -1.0f;
				//}
			}
		}
		
		if(numforces != 0)
			force /= (float)numforces;

		//NEWCODE: do constrain to vertical movement
		if(constrain_to_vertical)
		{
			force.x = force.y = 0;
		}

		//-----------------------------------------------------------------
		//check for sufficient convergence
		//-----------------------------------------------------------------
		if(force.length2() < 0.0001*0.0001)
			break;

		displacement += force * 0.3f;//0.1;//TEMP was 0.1
	}

	// int numsprings = 0;
	// for(int s=0; s<springspheresets.size(); ++s)
	// 	numsprings += (int)springspheresets[s].collpoints.size();
	// 
	// conPrint("springs took " + toString(num_iters_done) + " iterations to solve for " + toString(numsprings) + " springs"); 

	return displacement;
}



void PlayerPhysics::debugGetCollisionSpheres(const Vec4f& campos, std::vector<js::BoundingSphere>& spheres_out)
{
	spheres_out.resize(0);
	for(int s=0; s<3; ++s)//for each sphere in body
	{
		//-----------------------------------------------------------------
		//calc position of sphere
		//-----------------------------------------------------------------
		const Vec3f spherepos = Vec3f(campos) - Vec3f(0,0, (EYE_HEIGHT - 1.5f) + (float)s * 0.6f);
		spheres_out.push_back(js::BoundingSphere(spherepos.toVec4fPoint(), SPHERE_RAD));
	}
}
