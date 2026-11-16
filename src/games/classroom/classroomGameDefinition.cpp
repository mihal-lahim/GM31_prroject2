#include "gameDefinition.h"
#include "classroomWalk.h"

bool RunClassroomRenderTest();
bool RunAnimatorSelfTests();

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<ClassroomWalkScene>();
	}
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition{
		"Classroom Walk",
		"assets/config/classroom.cfg",
		nullptr,
		CreateInitialScene,
		RunAnimatorSelfTests,
		RunClassroomRenderTest
	};
	return definition;
}
