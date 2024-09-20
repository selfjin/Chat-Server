#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "Network.h"
#include "Define.h"
#include "Contents.h"
#include "SerializationBuffer.h"

//-------------------------------------------------------------
//  전역 변수
//-------------------------------------------------------------
Network g_Server;



void SOCK_ERROR_PRINT(const char* string)
{
    printf("Faild : %s,  ", string);
    int value = WSAGetLastError();
    printf("Error Code : %d\n", value);
}


///////////////////////////////////////////////////////////////

// 1. startUp And Run

bool Network::WSASet()
{

    WSAData wsaData;
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        SOCK_ERROR_PRINT("WSAStartUp()");
        return 0;
    }

    // 1. SOCKET

    Listen_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (Listen_Socket == INVALID_SOCKET)
    {
        SOCK_ERROR_PRINT("socket()");
        return 0;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    InetPton(AF_INET, L"0.0.0.0", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(SERVER_PORT);


    // 2. bind

    int bind_Check = bind(Listen_Socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));

    if (bind_Check == SOCKET_ERROR)
    {
        SOCK_ERROR_PRINT("bind");
        return 0;
    }


    // 3. Listen


    int listen_Check = listen(Listen_Socket, SOMAXCONN);

    if (listen_Check == SOCKET_ERROR)
    {
        SOCK_ERROR_PRINT("listen");
        return 0;
    }

    // 4. Socket Option


    // 4 - 1. Linger 

    LINGER linger_ON;
    {
        linger_ON.l_linger = 1;
        linger_ON.l_onoff = 0;
    }

    int linger_Check = setsockopt(Listen_Socket, SOL_SOCKET, SO_LINGER, (char*)&linger_ON, sizeof(linger_ON));

    if (linger_Check == SOCKET_ERROR)
    {
        SOCK_ERROR_PRINT("linger");
        return 0;
    }


    // 4- 2. NonBlocking 

    u_long nonBlocking_ON = 1;

    int ioctl_Check = ioctlsocket(Listen_Socket, FIONBIO, &nonBlocking_ON);

    if (ioctl_Check == SOCKET_ERROR)
    {
        SOCK_ERROR_PRINT("ioctl");
        return 0;
    }

    return 1;
}

bool Network::Run()
{
    return false;
}

void Network::NETWORK_UNICAST(char* packet, Session* user, PACKET_HEADER* header)
{
    if (user->Alive == false)
    {
        return;
    }

    user->sendBuffer->Enqueue((char*)header, sizeof(PACKET_HEADER));
    user->sendBuffer->Enqueue(packet, header->wPayloadSize);
}


void Network::NETWORK_BROADCAST(char* packet, Session* exclude_Session, PACKET_HEADER* header)
{
    // 브로드캐스트
    if (exclude_Session == nullptr)
    {
        auto it = playerList.begin();

        while (it != playerList.end())
        {
            if ((*it)->Alive != false)
            {
                (*it)->sendBuffer->Enqueue((char*)header, sizeof(PACKET_HEADER));
                (*it)->sendBuffer->Enqueue(packet, header->wPayloadSize);
            }
            ++it;
        }
    }
    else
    {
        auto it = playerList.begin();

        while (it != playerList.end())
        {
            if ((*it)->Alive != false && (*it)->Sock != exclude_Session->Sock)
            {
                (*it)->sendBuffer->Enqueue((char*)header, sizeof(PACKET_HEADER));
                (*it)->sendBuffer->Enqueue(packet, header->wPayloadSize);
            }
            ++it;
        }
    }
}

void Network::netIOProcess_LISTEN()
{
    FD_SET reads;
    FD_ZERO(&reads);

    timeval select_Non_Blocking;
    select_Non_Blocking.tv_sec = 0;
    select_Non_Blocking.tv_usec = 0;

    FD_SET(Listen_Socket, &reads);

    // 1. Listen Socket

    FD_SET(Listen_Socket, &reads);

    // 2. Select

    int recvSelect = select(0, &reads, NULL, NULL, &select_Non_Blocking);

    if (recvSelect == SOCKET_ERROR)
    {
        SOCK_ERROR_PRINT("select - Listen");
        DebugBreak;
    }

    if (FD_ISSET(Listen_Socket, &reads))
    {
        NETWORK_PROC_ACCEPT();
    }
}

void Network::netIOProcess_RECV()
{
    FD_SET reads;
    
    timeval select_Non_Blocking;
    select_Non_Blocking.tv_sec = 0;
    select_Non_Blocking.tv_usec = 0;

    //-------------------------------------------------------------
    // SET
    //-------------------------------------------------------------

    int totalUser = playerList.size();

    if (totalUser == 0)return;

    if (totalUser <= SELECT_MAX_SIZE)
    {
        //-------------------------------------------------------------
        // Pooling   /   SELECT_MAX_SIZE <= 64
        //-------------------------------------------------------------

        //1. read Set 초기화
        FD_ZERO(&reads);

        // 2. Player Socket
        for (auto it : playerList)
        {
            FD_SET((*it).Sock, &reads);
        }

        // 3. Select
        int recvSelect = select(0, &reads, NULL, NULL, &select_Non_Blocking);

#ifdef _DEBUG
        if (recvSelect == SOCKET_ERROR)
        {
            SOCK_ERROR_PRINT("select - recv");
            DebugBreak();
        }
#endif

        for (auto recvPlayerIter : playerList)
        {
            if (FD_ISSET(recvPlayerIter->Sock, &reads))
            {
                if ((*recvPlayerIter).Alive == false)
                {
                    continue;
                }

                char Message[MESSAGE_BUFFER_SIZE] = { 0, };

                int recvByte = recv((*recvPlayerIter).Sock, Message, MESSAGE_BUFFER_SIZE, 0);

                if (recvByte == SOCKET_ERROR)
                {
                    if (WSAGetLastError() == WSAEWOULDBLOCK)
                    {
                        continue;
                    }
                    else
                    {
                        (*recvPlayerIter).Alive = false;
                        continue;
                    }
                }

                if (recvByte == 0)
                {
                    (*recvPlayerIter).Alive = false;
                    continue;
                }

                (*recvPlayerIter).recvBuffer->Enqueue(Message, recvByte);

                while (1)
                {
                    PACKET_HEADER header;
                    int headerPeekSize = (*recvPlayerIter).recvBuffer->peek((char*)&header, sizeof(PACKET_HEADER));

                    if (headerPeekSize < sizeof(header))
                    {
                        break;
                    }

                    if (header.byCode != dfPACKET_CODE)
                    {
                        (*recvPlayerIter).Alive = false;
                        break;
                    }

                    NETWORK_PROC(&header, recvPlayerIter);

                    if ((*recvPlayerIter).recvBuffer->getSize() < sizeof(PACKET_HEADER))
                    {
                        break;
                    }
                }
            }

            
        }
    }
    else
    {
        //-------------------------------------------------------------
        // Pooling   /   SELECT_MAX_SIZE > 64
        //-------------------------------------------------------------
        int poolingCount = totalUser / SELECT_MAX_SIZE;
        int poolingRemain = totalUser % SELECT_MAX_SIZE;


        for (int i = 0; i < poolingCount; i++)
        {
            FD_ZERO(&reads);
            auto playerIterSet = playerList.begin();
            std::advance(playerIterSet, i * SELECT_MAX_SIZE);

            for (int inRange = 0; inRange < SELECT_MAX_SIZE; inRange++)
            {
                FD_SET((*playerIterSet)->Sock, &reads);
                playerIterSet++;
            }

            // 3. Select 처리
            int recvSelect = select(0, &reads, NULL, NULL, &select_Non_Blocking);

#ifdef _DEBUG
            if (recvSelect == SOCKET_ERROR)
            {
                SOCK_ERROR_PRINT("select - recv");
                DebugBreak();
            }
#endif
            auto playerIterIsSet = playerList.begin();
            std::advance(playerIterIsSet, i * SELECT_MAX_SIZE);

            for (int isSet = 0; i < SELECT_MAX_SIZE; isSet++)
            {
                if (FD_ISSET((*playerIterIsSet)->Sock, &reads))
                {
                    if ((*playerIterIsSet)->Alive != false)
                    {
                        continue;
                    }

                    char Message[MESSAGE_BUFFER_SIZE] = { 0, };

                    int recvByte = recv((*playerIterIsSet)->Sock, Message, MESSAGE_BUFFER_SIZE, 0);

                    if (recvByte == SOCKET_ERROR)
                    {
                        if (WSAGetLastError() == WSAEWOULDBLOCK)
                        {
                            continue;
                        }
                        else
                        {
                            (*playerIterIsSet)->Alive = false;
                            continue;
                        }
                    }

                    if (recvByte == 0)
                    {
                        (*playerIterIsSet)->Alive = false;
                        continue;
                    }

                    (*playerIterIsSet)->recvBuffer->Enqueue(Message, recvByte);

                    while (1)
                    {
                        PACKET_HEADER header;
                        int headerPeekSize = (*playerIterIsSet)->recvBuffer->peek((char*)&header, sizeof(PACKET_HEADER));

                        if (headerPeekSize < sizeof(header))
                        {
                            break;
                        }

                        if (header.byCode != dfPACKET_CODE)
                        {
                            (*playerIterIsSet)->Alive = false;
                            break;
                        }

                        NETWORK_PROC(&header, *playerIterIsSet);

                        if ((*playerIterIsSet)->recvBuffer->getSize() < sizeof(PACKET_HEADER))
                        {
                            break;
                        }
                    }

                }

                playerIterIsSet++;
            }


        }

        if (poolingRemain > 0)
        {
            FD_ZERO(&reads);
            auto reMainIter = playerList.begin();
            std::advance(reMainIter, poolingCount * SELECT_MAX_SIZE); // 남은 소켓의 시작점으로 이동
            for (int j = 0; j < poolingRemain; j++)
            {
                FD_SET((*reMainIter)->Sock, &reads); // 남은 소켓을 FD_SET에 등록
                reMainIter++;
            }

            int remainSelect = select(0, &reads, NULL, NULL, &select_Non_Blocking);

#ifdef _DEBUG
            if (remainSelect == SOCKET_ERROR)
            {
                DebugBreak();
            }
#endif 

            std::advance(reMainIter, -poolingRemain);

            for (int i = 0; i < poolingRemain; i++)
            {
                if (FD_ISSET((*reMainIter)->Sock, &reads))
                {
                    if ((*reMainIter)->Alive == false)
                    {
                        continue;
                    }

                    char Message[MESSAGE_BUFFER_SIZE] = { 0, };

                    int recvByte = recv((*reMainIter)->Sock, Message, MESSAGE_BUFFER_SIZE, 0);

                    if (recvByte == SOCKET_ERROR)
                    {
                        if (WSAGetLastError() == WSAEWOULDBLOCK)
                        {
                            continue;
                        }
                        else
                        {
                            (*reMainIter)->Alive = false;
                            continue;
                        }
                    }

                    if (recvByte == 0)
                    {
                        (*reMainIter)->Alive = false;
                        continue;
                    }

                    (*reMainIter)->recvBuffer->Enqueue(Message, recvByte);

                    while (1)
                    {
                        PACKET_HEADER header;
                        int headerPeekSize = (*reMainIter)->recvBuffer->peek((char*)&header, sizeof(PACKET_HEADER));

                        if (headerPeekSize < sizeof(header))
                        {
                            break;
                        }

                        if (header.byCode != dfPACKET_CODE)
                        {
                            (*reMainIter)->Alive = false;
                            break;
                        }

                        NETWORK_PROC(&header, *reMainIter);

                        if ((*reMainIter)->recvBuffer->getSize() < sizeof(PACKET_HEADER))
                        {
                            break;
                        }
                    }

                }
                reMainIter++;
            }

        }  
    }
}

void Network::netIOProcess_SEND()
{
    fd_set writes;
    FD_ZERO(&writes);

    timeval select_Non_Blocking;
    select_Non_Blocking.tv_sec = 0;
    select_Non_Blocking.tv_usec = 0;

    //-------------------------------------------------------------
    // SET
    //-------------------------------------------------------------

    // 1. Player Socket
    for (auto it : playerList)
    {
        if ((*it).sendBuffer->getSize())
        {
            FD_SET((*it).Sock, &writes);
        }
    }

    // 2. Select

    if (writes.fd_count != 0)
    {
        int recvSelect = select(0, NULL, &writes, NULL, &select_Non_Blocking);

        if (recvSelect == SOCKET_ERROR)
        {
            SOCK_ERROR_PRINT("select - recv");
            DebugBreak;
        }
    }

    //-------------------------------------------------------------
    // Pooling
    //-------------------------------------------------------------

    for (auto it : playerList)
    {
        if (FD_ISSET((*it).Sock, &writes))
        {
            if ((*it).Alive == false)
            {
                continue;
            }
            char message[MESSAGE_BUFFER_SIZE] = { 0, };

            int peekSize = (*it).sendBuffer->peek(message, (*it).sendBuffer->getSize());

            int sendByteMessage = send((*it).Sock, message, peekSize, 0);

            if (sendByteMessage == SOCKET_ERROR)
            {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                {
                    (*it).Alive = false;
                    continue;
                }
                else
                {
                    (*it).Alive = false;
                    continue;
                }
            }

            if (sendByteMessage == 0)
            {
                (*it).Alive == false;
                continue;
            }

            //(*it).sendBuffer->moveBegin(peekSize);
            (*it).sendBuffer->Dequeue(message, peekSize);
        }
    }
}




///////////////////////////////////////////////////////////////

// 2. Procedure


void Network::NET_PACKET_MP_HEADER(PACKET_HEADER* header, CPacket* payload, WORD msgType, WORD payLoadSize)
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


void Network::NETWORK_PROC_ACCEPT()
{
    SOCKET Client_Socket;
    SOCKADDR_IN clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));

    int addrLen = sizeof(clientAddr);

    Client_Socket = accept(Listen_Socket, (SOCKADDR*)&clientAddr, &addrLen);

    if (Client_Socket == INVALID_SOCKET)
    {
        SOCK_ERROR_PRINT("accept");
        return;
    }

    Session* createUserSession = new Session();
    {
        InetNtop(AF_INET, &clientAddr.sin_addr, createUserSession->Ip, 16);
        createUserSession->Port = ntohs(clientAddr.sin_port);
        createUserSession->Sock = Client_Socket;
        createUserSession->SessoinID = Session_ID_NUMBER++;
        createUserSession->Alive = true;
    }

    playerList.push_back(createUserSession);

}

void Network::NETWORK_PROC(PACKET_HEADER* header, Session* session)
{

    switch (header->wMsgType)
    {
    case df_REQ_LOGIN:
    {
        CPacket packet;
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);

        if (wcslen((WCHAR*)packet.GetBufferPtr()) > dfNICK_MAX_LEN - 1)
        {
            PACKET_HEADER sendHeader;
            CPacket sendPacket;
            NET_PACKET_MP_LOGIN_RES(&sendPacket, df_RESULT_LOGIN_ETC, session->SessoinID);


            NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RESULT_LOGIN_ETC, sendPacket.getSize());
            NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);
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
                NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);
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
                NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);

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
        NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);

        break;
    }
    case df_REQ_ROOM_CREATE:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        CPacket packet;
        session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);

        PACKET_HEADER sendHeader;
        CPacket sendPacket;

        
        int ret = RoomCreate(&packet, &sendPacket);
        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_CREATE, sendPacket.getSize());
        NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);

        // 생성 성공시 다른 유저들에게 알림
        if (ret == df_RESULT_ROOM_CREATE_OK)
        {
            for (auto it : Contents_Player)
            {
                if (it.second->mySession->SessoinID != session->SessoinID && it.second->mySession->Alive != false)
                {
                    NETWORK_UNICAST(sendPacket.GetBufferPtr(), it.second->mySession, &sendHeader);
                }
            }
        }

        break;
    }
    case df_REQ_ROOM_ENTER:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        CPacket packet;
        session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);

        PACKET_HEADER sendHeader;
        CPacket sendPacket;

        int roomNumOut = 0;
        int ret = RoomVisited(&packet, &sendPacket, session->SessoinID, &roomNumOut);
        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_ENTER, sendPacket.getSize());

        NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);

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
                    NETWORK_UNICAST(sendPacket.GetBufferPtr(), Contents_Player[it]->mySession, &sendHeader);
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
            NETWORK_UNICAST(sendPacket.GetBufferPtr(), Contents_Player[it]->mySession, &sendHeader);
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
                if (it.second->loginState == false)
                {
                    continue;
                }
                NETWORK_UNICAST(sendPacket.GetBufferPtr(), it.second->mySession, &sendHeader);
            }
        }
        
        break;
    }
    case df_REQ_CHAT:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        CPacket packet;
        session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);

        
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
                NETWORK_UNICAST(sendPacket.GetBufferPtr(), Contents_Player[it]->mySession, &sendHeader);
            }
        }

        break;
    }
    case df_REQ_STRESS_ECHO:
    {
        session->recvBuffer->moveBegin(sizeof(PACKET_HEADER));
        CPacket packet;
        session->recvBuffer->Dequeue(packet.GetBufferPtr(), header->wPayloadSize);

        PACKET_HEADER sendHeader;
        CPacket sendPacket;

        EhcoRogic(&packet, &sendPacket);
        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_STRESS_ECHO, sendPacket.getSize());

        NETWORK_UNICAST(sendPacket.GetBufferPtr(), session, &sendHeader);

        break;
    }

    default:
        break;
    }
}





void Network::SessionAdvisor()
{

    auto deletePlayer = playerList.begin();


    while (deletePlayer != playerList.end())
    {
        if ((*deletePlayer)->Alive == false)
        {

            auto playerSearchIter = Contents_Player_Search.find((*deletePlayer)->SessoinID);
            if (playerSearchIter != Contents_Player_Search.end())
            {
                
                if (Contents_Player[Contents_Player_Search[(*deletePlayer)->SessoinID]]->roomVisited)
                {
                    Contents_Player[Contents_Player_Search[(*deletePlayer)->SessoinID]]->loginState = false;

                    PACKET_HEADER sendHeader;
                    CPacket sendPacket;

                    // 유저가 속한 방의 No
                    int outRoomNum = Contents_Player[Contents_Player_Search[(*deletePlayer)->SessoinID]]->RoomState;

                    RoomLeave(&sendPacket, (*deletePlayer)->SessoinID);
                    NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_LEAVE, sendPacket.getSize());

                    for (auto& it : Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList)
                    {
                        NETWORK_UNICAST(sendPacket.GetBufferPtr(), Contents_Player[it]->mySession, &sendHeader);
                    }

                    // 해당 유저를 방에서 삭제, 그러나 그 유저가 마지막 유저였다면, 방 또한 삭제함.

                    //

                    auto deleteUserIterator = std::find(
                        Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList.begin(),
                        Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList.end(),
                        Contents_Player_Search[(*deletePlayer)->SessoinID]
                    );
                    Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList.erase(deleteUserIterator);

                    // 유저 목록에서 유저 제거

                    Contents_Player_Search.erase((*deletePlayer)->SessoinID);
                    Contents_Player.erase(Contents_Player_Search[(*deletePlayer)->SessoinID]);

                    if (Contents_Room[Contents_Room_Search[outRoomNum]].playerNameList.size() == 0)
                    {
                        // 방 삭제 패킷 전 인원에게 전송
                        sendPacket.Clear();

                        RoomDelete(&sendPacket, outRoomNum);
                        NET_PACKET_MP_HEADER(&sendHeader, &sendPacket, df_RES_ROOM_DELETE, sendPacket.getSize());

                        for (auto it : Contents_Player)
                        {
                            NETWORK_UNICAST(sendPacket.GetBufferPtr(), it.second->mySession, &sendHeader);
                        }
                    }

                }
            }

            delete(*deletePlayer);
            deletePlayer = playerList.erase(deletePlayer);

            
        }
        else
        {
            deletePlayer++;
        }

    }


}
