#pragma once

#include "Network.h"
#include <map>


//-------------------------------------------------------------
//  struct
//-------------------------------------------------------------

struct RoomState
{
	int roomNumber;
	std::wstring RoomName;
	std::list<std::wstring> playerNameList;

	bool RoomAlive = true;
	
};

//-------------------------------------------------------------
//  extern
//-------------------------------------------------------------


extern std::unordered_map<std::wstring , Player*> Contents_Player;

extern std::map<std::wstring, RoomState> Contents_Room;

