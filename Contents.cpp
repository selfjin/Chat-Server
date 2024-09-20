#include "Contents.h"
#include "Network.h"



std::unordered_map<std::wstring , Player*> Contents_Player;
std::map<int, std::wstring> Contents_Player_Search;

std::map<std::wstring, RoomState> Contents_Room;
std::unordered_map<int, std::wstring> Contents_Room_Search;


int g_RoomNumber = 1;


//-------------------------------------------------------------
//  NET_PACKET_MP_PROC  // Make Packet
//-------------------------------------------------------------


void NET_PACKET_MP_LOGIN_RES(CPacket* MakePacket, BYTE reserve, int ID)
{
	*MakePacket << reserve;
	*MakePacket << ID;
}

void NET_PACKET_MP_ROOM_LIST(CPacket* MakePacket, short roomNum)
{
	*MakePacket << roomNum;


	for (auto& it : Contents_Room)
	{
		*MakePacket << it.second.roomNumber;
		*MakePacket << (short)(it.second.RoomName.size() * sizeof(WCHAR));


		for (const auto& wchar : it.second.RoomName)
		{
			*MakePacket << wchar;
		}

		*MakePacket << (BYTE)it.second.playerNameList.size();

		for (auto playerName = it.second.playerNameList.begin();
			playerName != it.second.playerNameList.end();
			++playerName)
		{

			WCHAR nameData[dfNICK_MAX_LEN] = { 0, };
			wcscpy_s(nameData, dfNICK_MAX_LEN, (*playerName).c_str());

			for (int n = 0; n < dfNICK_MAX_LEN; n++)
			{
				*MakePacket << nameData[n];
			}
		}

	}
}

//-------------------------------------------------------------
//  Logic 
//-------------------------------------------------------------


int RoomCreate(CPacket* payload, CPacket* sendPacket)
{
	short roomNameSize;
	*payload >> roomNameSize;

	WCHAR Room[1000] = { 0, };
	for (int i = 0; i < roomNameSize / 2; i++)
	{
		*payload >> Room[i];
	}

	Room[wcslen(Room)] = L'\0';

	if (Contents_Room.find(Room) == Contents_Room.end())  // ���� ������ ����
	{
		Contents_Room[Room].RoomAlive = true;
		Contents_Room[Room].roomNumber = g_RoomNumber++;
		Contents_Room[Room].RoomName = Room;

		Contents_Room_Search[Contents_Room[Room].roomNumber] = Contents_Room[Room].RoomName;

		*sendPacket << (BYTE)df_RESULT_ROOM_CREATE_OK;
		*sendPacket << (int)Contents_Room[Room].roomNumber;
		*sendPacket << (short)roomNameSize;

		for (int i = 0; i < roomNameSize / 2; i++)
		{
			*sendPacket << Room[i];
		}


		return df_RESULT_ROOM_CREATE_OK;
	}
	else
	{
		*sendPacket << (BYTE)df_RESULT_ROOM_CREATE_DNICK;
		*sendPacket << (int)Contents_Room[Room].roomNumber;
		*sendPacket << (short)roomNameSize;

		for (int i = 0; i < roomNameSize / 2; i++)
		{
			*sendPacket << Room[i];
		}

		return df_RESULT_ROOM_CREATE_DNICK;
	}

}

int RoomVisited(CPacket* payload, CPacket* sendPacket, int searchID, int* roomNumOut)
{
	int roomNum;

	*payload >> roomNum;
	*roomNumOut = roomNum;

	if (Contents_Player[Contents_Player_Search[searchID]]->roomVisited)
	{
		*sendPacket << (BYTE)df_RESULT_ROOM_ENTER_NOT;
		return df_RESULT_ROOM_ENTER_NOT;
	}

	Contents_Room[Contents_Room_Search[roomNum]].playerNameList.push_back(
		Contents_Player_Search[searchID]);

	Contents_Player[Contents_Player_Search[searchID]]->RoomState = roomNum;
	Contents_Player[Contents_Player_Search[searchID]]->roomVisited = true;

	auto it = Contents_Room.begin();

	if (it != Contents_Room.end())
	{

		if ((*it).second.roomNumber == roomNum)
		{
			
			*sendPacket << (BYTE)df_RESULT_ROOM_ENTER_OK;
			*sendPacket << (int)roomNum;
			*sendPacket << (short)((*it).second.RoomName.size() * sizeof(WCHAR));
			
			for (int i = 0; i < (*it).second.RoomName.size(); i++)
			{
				*sendPacket << (WCHAR)(*it).second.RoomName[i];
			}

			*sendPacket << (BYTE)(*it).second.playerNameList.size();

			for (auto playerName = (*it).second.playerNameList.begin();
				playerName != (*it).second.playerNameList.end();
				++playerName)
			{

				WCHAR nameData[dfNICK_MAX_LEN] = { 0, };
				wcscpy_s(nameData, dfNICK_MAX_LEN, (*playerName).c_str());

				for (int n = 0; n < dfNICK_MAX_LEN; n++)
				{
					*sendPacket << nameData[n];
				}

				*sendPacket << Contents_Player[*playerName]->mySession->SessoinID;
			}

			return df_RESULT_ROOM_ENTER_OK;
		}
		else
		{
			it++;
		}
		
	}

	*sendPacket << (BYTE)df_RESULT_ROOM_ENTER_NOT;
	return df_RESULT_ROOM_ENTER_NOT;

}




void MP_OtherUser(CPacket* sendPacket, std::wstring userName, int otherID)
{
	WCHAR input[dfNICK_MAX_LEN] = { 0, };

	wcscpy_s(input, dfNICK_MAX_LEN, userName.c_str());


	for (int i = 0; i < dfNICK_MAX_LEN; i++)
	{
		*sendPacket << (WCHAR)input[i];
	}

	*sendPacket << (int)otherID;

}

void RoomLeave(CPacket* sendPacket, int leaveID)
{
	Contents_Player[Contents_Player_Search[leaveID]]->roomVisited = false;
	Contents_Player[Contents_Player_Search[leaveID]]->RoomState = 0;

	*sendPacket << (int)leaveID;
}

void RoomDelete(CPacket* sendPacket, int roomNum)
{

	auto deleteRoomIterator = Contents_Room.find(Contents_Room_Search[roomNum]);

	if (deleteRoomIterator != Contents_Room.end())
	{
		Contents_Room.erase(deleteRoomIterator);
	}

	*sendPacket << (int)roomNum;
}


void RoomMessagePacket(CPacket* payload, CPacket* sendPacket, int senderID)
{
	short recvMessageSize;

	WCHAR message[1000];

	*payload >> recvMessageSize;
	
	for (int i = 0; i < recvMessageSize / 2; i++)
	{
		*payload >> message[i];
	}

	*sendPacket << (int)senderID;
	*sendPacket << (short)recvMessageSize;


	for (int i = 0; i < recvMessageSize / 2; i++)
	{
		*sendPacket << (WCHAR)message[i];
	}
	
}

void EhcoRogic(CPacket* payload, CPacket* sendPacket)
{
	WORD recvMessageSize;

	*payload >> recvMessageSize;



}