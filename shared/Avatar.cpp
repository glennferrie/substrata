/*=====================================================================
Avatar.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2016-01-12 12:24:54 +1300
=====================================================================*/
#include "Avatar.h"


#if GUI_CLIENT
#include "opengl/OpenGLEngine.h"
#include "../gui_client/AvatarGraphics.h"
#endif
#include <utils/ConPrint.h>
#include <utils/StringUtils.h>
#include <utils/IncludeXXHash.h>


Avatar::Avatar()
{
	transform_dirty = false;
	other_dirty = false;
	//opengl_engine_ob = NULL;
	//using_placeholder_model = false;

	next_snapshot_i = 0;
//	last_snapshot_time = 0;

	selected_object_uid = UID::invalidUID();

#if GUI_CLIENT
	name_colour = Colour3f(0.8f);
#endif
	anim_state = 0;
}


Avatar::~Avatar()
{}


void Avatar::generatePseudoRandomNameColour()
{
#if GUI_CLIENT
	// Assign a pseudo-random name colour to the avatar
	const uint64 hash_r = XXH64(name.c_str(), name.size(), /*seed=*/1);
	const uint64 hash_g = XXH64(name.c_str(), name.size(), /*seed=*/2);
	const uint64 hash_b = XXH64(name.c_str(), name.size(), /*seed=*/3);

	name_colour.r = (float)((double)hash_r / (double)std::numeric_limits<uint64>::max()) * 0.7f;
	name_colour.g = (float)((double)hash_g / (double)std::numeric_limits<uint64>::max()) * 0.7f;
	name_colour.b = (float)((double)hash_b / (double)std::numeric_limits<uint64>::max()) * 0.7f;

	// conPrint("Generated name_colour=" + name_colour.toVec3().toString() + " for avatar " + name);
#endif
}


void Avatar::appendDependencyURLs(std::vector<std::string>& URLs_out)
{
	URLs_out.push_back(model_url);
}


void Avatar::setTransformAndHistory(const Vec3d& pos_, const Vec3f& rotation_)
{
	pos = pos_;
	rotation = rotation_;

	for(int i=0; i<HISTORY_BUF_SIZE; ++i)
	{
		pos_snapshots[i] = pos_;
		rotation_snapshots[i] = rotation_;
		snapshot_times[i] = 0;
	}
}


void Avatar::getInterpolatedTransform(double cur_time, Vec3d& pos_out, Vec3f& rotation_out) const
{
	/*
	Timeline: check marks are snapshots received:

	|---------------|----------------|---------------|----------------|
	                                                                       ^
	                                                                      cur_time
	                                                                  ^
	                                               ^                last snapshot
	                                             cur_time - send_period * delay_factor

	*/

	const double send_period = 0.1; // Time between update messages from server
	const double delay = send_period * 2.0; // Objects are rendered using the interpolated state at this past time.

	const double delayed_time = cur_time - delay;
	// Search through history for first snapshot
	int begin = 0;
	for(int i=(int)next_snapshot_i-HISTORY_BUF_SIZE; i<(int)next_snapshot_i; ++i)
	{
		const int modi = Maths::intMod(i, HISTORY_BUF_SIZE);
		if(snapshot_times[modi] > delayed_time)
		{
			begin = Maths::intMod(modi - 1, HISTORY_BUF_SIZE);
			break;
		}
	}

	const int end = Maths::intMod(begin + 1, HISTORY_BUF_SIZE);

	float t;
	if(snapshot_times[end] == snapshot_times[begin])
		t = 0;
	else
		t  = (float)((delayed_time - snapshot_times[begin]) / (snapshot_times[end] - snapshot_times[begin])); // Interpolation fraction

	pos_out      = Maths::uncheckedLerp(pos_snapshots[begin], pos_snapshots[end], t);
	rotation_out = Maths::uncheckedLerp(rotation_snapshots[begin], rotation_snapshots[end], t);

	//const double send_period = 0.1; // Time between update messages from server
	//const double delay = /*send_period * */2.0; // Objects are rendered using the interpolated state at this past time.  In normalised period coordinates.

	//const int last_snapshot_i = next_snapshot_i - 1;

	//const double frac = (cur_time - last_snapshot_time) / send_period; // Fraction of send period ahead of last_snapshot cur time is
	////printVar(frac);
	//const double delayed_state_pos = (double)last_snapshot_i + frac - delay; // Delayed state position in normalised period coordinates.
	//const int delayed_state_begin_snapshot_i = myClamp(Maths::floorToInt(delayed_state_pos), last_snapshot_i - HISTORY_BUF_SIZE + 1, last_snapshot_i);
	//const int delayed_state_end_snapshot_i   = myClamp(delayed_state_begin_snapshot_i + 1,   last_snapshot_i - HISTORY_BUF_SIZE + 1, last_snapshot_i);
	//const float t  = delayed_state_pos - delayed_state_begin_snapshot_i; // Interpolation fraction

	//const int begin = Maths::intMod(delayed_state_begin_snapshot_i, HISTORY_BUF_SIZE);
	//const int end   = Maths::intMod(delayed_state_end_snapshot_i,   HISTORY_BUF_SIZE);

	//pos_out   = Maths::uncheckedLerp(pos_snapshots  [begin], pos_snapshots  [end], t);
	//axis_out  = Maths::uncheckedLerp(axis_snapshots [begin], axis_snapshots [end], t);
	//angle_out = Maths::uncheckedLerp(angle_snapshots[begin], angle_snapshots[end], t);

	//if(axis_out.length2() < 1.0e-10f)
	//{
	//	axis_out = Vec3f(0,0,1);
	//	angle_out = 0;
	//}
}



void writeToNetworkStream(const Avatar& avatar, OutStream& stream) // Write without version
{
	writeToStream(avatar.uid, stream);
	stream.writeStringLengthFirst(avatar.name);
	stream.writeStringLengthFirst(avatar.model_url);
	writeToStream(avatar.pos, stream);
	writeToStream(avatar.rotation, stream);
}


void readFromNetworkStreamGivenUID(InStream& stream, Avatar& avatar) // UID will have been read already
{
	avatar.name			= stream.readStringLengthFirst(10000);
	avatar.model_url	= stream.readStringLengthFirst(10000);
	avatar.pos			= readVec3FromStream<double>(stream);
	avatar.rotation		= readVec3FromStream<float>(stream);
}
