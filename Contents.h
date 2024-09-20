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

extern std::map<int, std::wstring> Contents_Player_Search;

extern std::unordered_map<int, std::wstring> Contents_Room_Search;

//-------------------------------------------------------------
//  NET_PACKET_MP_PROC  // Make Packet
//-------------------------------------------------------------

void NET_PACKET_MP_LOGIN_RES(CPacket* MakePacket, BYTE reserve, int ID);

void NET_PACKET_MP_ROOM_LIST(CPacket* MakePacket, short roomNum);


//-------------------------------------------------------------
//  Logic 
//-------------------------------------------------------------


int RoomCreate(CPacket* payload, CPacket* sendPacket);

int RoomVisited(CPacket* payload, CPacket* sendPacket, int searchID, int* roomNumOut);

void MP_OtherUser(CPacket* sendPacket, std::wstring userName, int otherID);

void RoomLeave(CPacket* sendPacket, int leaveID);

void RoomDelete(CPacket* sendPacket, int roomNum);

void RoomMessagePacket(CPacket* payload, CPacket* sendPacket, int senderID);