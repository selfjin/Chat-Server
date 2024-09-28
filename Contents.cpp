#include "Contents.h"
#include "Network.h"



std::unordered_map<std::wstring , Player*> Contents_Player;
std::map<int, std::wstring> Contents_Player_Search;

std::map<std::wstring, RoomState> Contents_Room;
std::unordered_map<int, std::wstring> Contents_Room_Search;



int g_RoomNumber = 1;


//-------------------------------------------------------------
//  Packet Procedure
//-------------------------------------------------------------

void NETWORK_PROC(PACKET_HEADER* header, Session* session)
{

    if (header->wPayloadSize + sizeof(PACKET_HEADER) > session->recvBuffer->getSize())
    {
        return;
    }

    switch (header->wMsgType)
    {
    case df_REQ_LOGIN:
    {
        CPacket packet;
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        int value = session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);
        packet.moveEnd(value);

        if (wcslen((WCHAR*)packet.GetBufferPtr()) > dfNICK_MAX_LEN - 1)
        {
            PACKET_HEADER sendHeader;
            CPacket sendPacket;
            NET_PACKET_MP_LOGIN_RES(&sendPacket, df_RESULT_LOGIN_ETC, session->SessoinID);


            NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RESULT_LOGIN_ETC, sendPacket.getSize());
            g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);
        }
        else
        {
            WCHAR userName[dfNICK_MAX_LEN] = { 0, };
            wcscpy_s(userName, dfNICK_MAX_LEN, (WCHAR*)packet.GetBufferPtr());
            userName[wcslen(userName)] = L'\0';

            auto it = Contents_Player.find(userName);


            // 중복되는 닉네임이 없을 경우              df_RESULT_LOGIN_OK
            if (it == Contents_Player.end())
            {
                PACKET_HEADER sendHeader;
                CPacket sendPacket;

                NET_PACKET_MP_LOGIN_RES(&sendPacket, df_RESULT_LOGIN_OK, session->SessoinID);

                NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_LOGIN, sendPacket.getSize());
                g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);
                Contents_Player[userName] = new Player(session);
                Contents_Player[userName]->loginState = true;
                Contents_Player_Search[Contents_Player[userName]->mySession->SessoinID] = userName;
            }
            else                        // 중복되는 닉네임이 있을 경우
            {
                PACKET_HEADER sendHeader;
                CPacket sendPacket;
                NET_PACKET_MP_LOGIN_RES(&sendPacket, df_RESULT_LOGIN_DNICK, session->SessoinID);

                NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RESULT_LOGIN_DNICK, sendPacket.getSize());
                g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);

                // False 해야 함
            }

        }

        break;
    }
    case df_REQ_ROOM_LIST:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));

        PACKET_HEADER sendHeader;
        CPacket sendPacket;

        NET_PACKET_MP_ROOM_LIST(&sendPacket, Contents_Room.size());

        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_LIST, sendPacket.getSize());
        g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);

        break;
    }
    case df_REQ_ROOM_CREATE:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        CPacket packet;
        int value = session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);
        packet.moveEnd(value);


        PACKET_HEADER sendHeader;
        CPacket sendPacket;

        int ret = RoomCreate(&packet, &sendPacket);
        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_CREATE, sendPacket.getSize());
        g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);

        // 생성 성공시 다른 유저들에게 알림
        if (ret == df_RESULT_ROOM_CREATE_OK)
        {
            for (auto it : Contents_Player)
            {
                if (it.second->mySession->SessoinID != session->SessoinID && it.second->mySession->Alive != false)
                {
                    g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), it.second->mySession, &sendHeader);
                }
            }
        }

        break;
    }
    case df_REQ_ROOM_ENTER:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        CPacket packet;
        int value = session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);
        packet.moveEnd(value);


        PACKET_HEADER sendHeader;
        CPacket sendPacket;

        int roomNumOut = 0;
        int ret = RoomVisited(&packet, &sendPacket, session->SessoinID, &roomNumOut);
        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_ENTER, sendPacket.getSize());

        g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);

        if (ret == df_RESULT_ROOM_ENTER_OK)
        {
            sendPacket.Clear();

            MP_OtherUser(&sendPacket, Contents_Player_Search[session->SessoinID],
                session->SessoinID);
            NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_USER_ENTER, sendPacket.getSize());

            for (auto& it : Contents_Room[Contents_Room_Search[roomNumOut]].playerNameList)
            {
                if (Contents_Player[it]->mySession->SessoinID != session->SessoinID &&
                    Contents_Player[it]->mySession->Alive != false)
                {
                    g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), Contents_Player[it]->mySession, &sendHeader);
                }
            }
        }

        break;
    }
    case df_REQ_ROOM_LEAVE:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));

        PACKET_HEADER sendHeader;
        CPacket sendPacket;

        // 유저가 속한 방의 No
        int outRoomNum = Contents_Player[Contents_Player_Search[session->SessoinID]]->RoomState;

        RoomLeave(&sendPacket, session->SessoinID);
        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_LEAVE, sendPacket.getSize());

        for (auto& it : Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList)
        {
            g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), Contents_Player[it]->mySession, &sendHeader);
        }

        //해당 유저를 방에서 삭제, 그러나 그 유저가 마지막 유저였다면, 방 또한 삭제함.

        auto deleteUserIterator = std::find(
            Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList.begin(),
            Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList.end(),
            Contents_Player_Search[session->SessoinID]
        );
        Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList.erase(deleteUserIterator);


        if (Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList.size() == 0)
        {
            // 방 삭제 패킷 전 인원에게 전송
            sendPacket.Clear();

            RoomDelete(&sendPacket, outRoomNum);
            NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_DELETE, sendPacket.getSize());

            for (auto it : Contents_Player)
            {
                if (it.second->loginState == false || it.second->mySession->Alive == false)
                {
                    continue;
                }
                g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), it.second->mySession, &sendHeader);
            }
        }

        break;
    }
    case df_REQ_CHAT:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        CPacket packet;
        int value = session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);
        packet.moveEnd(value);

        // 방의 다른 유저들에게 채팅 내용을 전송해야 합니다.
        PACKET_HEADER sendHeader;
        CPacket sendPacket;

        //유저가 속한 방의 번호
        int outRoomNum = Contents_Player[Contents_Player_Search[session->SessoinID]]->RoomState;

        RoomMessagePacket(&packet, &sendPacket, session->SessoinID);
        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_CHAT, sendPacket.getSize());

        for (auto& it : Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList)
        {
            if (Contents_Player[it]->mySession->SessoinID != session->SessoinID)
            {
                g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), Contents_Player[it]->mySession, &sendHeader);
            }
        }

        break;
    }
    case df_REQ_STRESS_ECHO:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        CPacket packet;
        int value = session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);
        packet.moveEnd(value);


        PACKET_HEADER sendHeader;
        //CPacket sendPacket;

        //EhcoRogic(&packet, &sendPacket);
        NET_PACKET_MP_HEADER(&sendHeader, &packet, df_RES_STRESS_ECHO, packet.getSize());

        g_Server.NETWORK_UNICAST(packet.GetBufferPtr(), session, &sendHeader);

        break;
    }

    default:
        break;
    }
}


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


		MakePacket->Enqueue((char*)it.second.RoomName.c_str(), it.second.RoomName.size() * sizeof(WCHAR));

		/*for (const auto& wchar : it.second.RoomName)
		{
			*MakePacket << wchar;		// 직렬화 버퍼 사용 방식, 그러나 이 방식은 함수 호출이 너무 많음.
		}*/

		*MakePacket << (BYTE)it.second.playerNameList.size();

		for (auto playerName = it.second.playerNameList.begin();
			playerName != it.second.playerNameList.end();
			++playerName)
		{

			WCHAR nameData[dfNICK_MAX_LEN] = { 0, };
			wcscpy_s(nameData, dfNICK_MAX_LEN, (*playerName).c_str());

			/*for (int n = 0; n < dfNICK_MAX_LEN; n++)
			{
				*MakePacket << nameData[n];			// 직렬화 버퍼 사용 방식, 
			}*/

			MakePacket->Enqueue((char*)nameData, dfNICK_MAX_LEN * sizeof(WCHAR));
		}

	}
}

void NET_PACKET_MP_HEADER(PACKET_HEADER* header, CPacket* payload, WORD msgType, WORD payLoadSize)
{
    header->byCode = 0x89;
    BYTE temp = 0;

    temp += msgType;
    char* payloadCharPtr = payload->GetBufferPtr();
    for (int i = 0; i < payLoadSize; i++)
    {
        temp += payloadCharPtr[i];
    }

    temp = temp % 256;

    header->byCheckSum = temp;
    header->wMsgType = msgType;
    header->wPayloadSize = payLoadSize;
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

	if (Contents_Room.find(Room) == Contents_Room.end())  // 방이 없으면 생성
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

	//WCHAR message[1000];
	char message[1000] = { 0, };

	*payload >> recvMessageSize;
	
	/*for (int i = 0; i < recvMessageSize / 2; i++)
	{
		*payload >> message[i];				// 직렬화 버퍼 사용 방식
	}*/

	payload->Dequeue((char*)message, (int)recvMessageSize);

	*sendPacket << (int)senderID;
	*sendPacket << (short)recvMessageSize;

	sendPacket->Enqueue((char*)message, (int)recvMessageSize);

	/*for (int i = 0; i < recvMessageSize / 2; i++)
	{
		*sendPacket << (WCHAR)message[i];		// 직렬화 버퍼 사용 방식
	}*/
	
}

void EhcoRogic(CPacket* payload, CPacket* sendPacket)
{
	short recvMessageSize;

	*payload >> recvMessageSize;

	// payLoad 처리를 하는 개념으로, 에코 테스트에서 받은 것을 그대로 돌려보내지 않기 위해 별도로 하는 작업. // 성능은 더 느려집니다.
	char payloadMessageBox[1000] = { 0 , };


	payload->Dequeue(payloadMessageBox, (int)recvMessageSize);


	*sendPacket << recvMessageSize;

	sendPacket->Enqueue(payloadMessageBox, (int)recvMessageSize);
	

}

void CONTETNTS_PLAYER_ADVISOR(int sessionID)
{
    auto it = Contents_Player_Search.find(sessionID);
    
    if (it != Contents_Player_Search.end())
    {
        if (Contents_Player[it->second]->roomVisited != false)
        {
            int playerRoomNum = Contents_Player[it->second]->RoomState;
            
            auto Room = Contents_Room_Search.find(playerRoomNum);
            if (Room != Contents_Room_Search.end())
            {

                // 방에 다른 인원이 남았다면 나간 유저 전파
                if (Contents_Room[Room->second].playerNameList.size() - 1 > 0) // 해당 유저 제외
                {
                    PACKET_HEADER sendHeader;
                    CPacket sendPacket;
                    for (auto& user : Contents_Room[Room->second].playerNameList)
                    {
                        if (Contents_Player[user]->mySession->SessoinID == sessionID)
                        {
                            continue;
                        }
                        RoomLeave(&sendPacket, sessionID);
                        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_LEAVE, sendPacket.getSize());

                        g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), Contents_Player[user]->mySession, &sendHeader);
                    }

                    // 이후 방의 네임 리스트에서 해당 유저를 지워줘야 함

                    auto deleteUser = std::find
                    (Contents_Room[Room->second].playerNameList.begin(),
                        Contents_Room[Room->second].playerNameList.end(),
                        it->second);
                    Contents_Room[Room->second].playerNameList.erase(deleteUser);

                }
                // 방에 다른 인원이 남아있지 않다면 방 삭제 전파 이후, 방 삭제
                else
                {
                    PACKET_HEADER sendHeader;
                    CPacket sendPacket;

                    Contents_Room.erase(Contents_Room_Search[playerRoomNum]);
                    Contents_Room_Search.erase(playerRoomNum);

                    RoomDelete(&sendPacket, playerRoomNum);
                    NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_DELETE, sendPacket.getSize());

                    // 방 삭제 전파
                    for (auto& user : Contents_Player)
                    {
                        g_Server.NETWORK_UNICAST(sendPacket.GetBufferPtr(), user.second->mySession, &sendHeader);
                    }
                    
                    Contents_Room.erase(Room->second);
                    Contents_Room_Search.erase(Room);
                }
            }

            Contents_Player.erase(it->second);
            Contents_Player_Search.erase(it);

        }
    }
    else
    {
        return;
    }

}