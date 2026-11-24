#include "pch.h"
#include "Query.h"
#include "GUI/Fuser/Fuser.h"
#include "GUI/Fuser/Visuals/Draw/Players.h"
#include "GUI/Fuser/Visuals/Draw/Troopers.h"
#include "GUI/Fuser/Visuals/Draw/Camps.h"
#include "GUI/Fuser/Visuals/Draw/Sinners.h"
#include "GUI/Fuser/Visuals/Draw/XpOrbs.h"
#include "GUI/Fuser/Visuals/Draw/Powerups.h"
#include "GUI/Fuser/Visuals/Draw/CampTimers.h"
#include "GUI/Radar/Radar.h"

bool Query::IsUsingPlayers()
{
	if (Fuser::bMasterToggle && Draw_Players::bMasterToggle) return true;

	return false;
}

bool Query::IsUsingTroopers()
{
	if (Fuser::bMasterToggle && Draw_Troopers::bMasterToggle) return true;

	return false;
}

bool Query::IsUsingCamps()
{
	if (Fuser::bMasterToggle && Draw_Camps::bMasterToggle) return true;

	return false;
}

bool Query::IsUsingSinners()
{
	if (Fuser::bMasterToggle && Draw_Sinners::bMasterToggle) return true;

	return false;
}

bool Query::IsUsingXpOrbs()
{
	if (Fuser::bMasterToggle && Draw_XpOrbs::bMasterToggle) return true;

	return false;
}

bool Query::IsUsingPowerups()
{
	if (Fuser::bMasterToggle && Draw_Powerups::bMasterToggle) return true;

	return false;
}
bool Query::IsUsingObjectiveTimers()
{
	if (!Draw_CampTimers::bMasterToggle) return false;

	// Read for either consumer: the world tags and banner on the Fuser, or the
	// radar, which renders in its own window independent of the Fuser toggle.
	if (Fuser::bMasterToggle && (Draw_CampTimers::bWorld || Draw_CampTimers::bMidBossBanner)) return true;
	if (Radar::bMasterToggle && Draw_CampTimers::bRadar) return true;

	return false;
}
