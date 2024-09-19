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
    FD_ZERO(&reads);

    timeval select_Non_Blocking;
    select_Non_Blocking.tv_sec = 0;
    select_Non_Blocking.tv_usec = 0;

    //-------------------------------------------------------------
    // SET
    //-------------------------------------------------------------

    int totalUser = playerList.size();

    if (totalUser == 0)return;

    if (totalUser < SELECT_MAX_SIZE)
    {
        //-------------------------------------------------------------
        // Pooling   /   SELECT_MAX_SIZE < 64
        //-------------------------------------------------------------

        // 2. Player Socket
        for (auto it : playerList)
        {
            FD_SET((*it).Sock, &reads);
        }

        // 3. Select
        int recvSelect = select(0, &reads, NULL, NULL, &select_Non_Blocking);

        if (recvSelect == SOCKET_ERROR)
        {
            SOCK_ERROR_PRINT("select - recv");
            DebugBreak();
        }

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

        FD_ZERO(&reads);
        for (int i = 0; i < poolingCount; i++)
        {
            auto playerIter = playerList.begin();
            std::advance(playerIter, i * SELECT_MAX_SIZE);
            for (int inRange = 0; inRange < SELECT_MAX_SIZE; inRange++)
            {
                FD_SET((*playerIter)->Sock, &reads);
                playerIter++;
            }
        }

        if (poolingRemain > 0)
        {
            auto playerIter = playerList.begin();
            std::advance(playerIter, poolingCount * SELECT_MAX_SIZE); // 남은 소켓의 시작점으로 이동
            for (int j = 0; j < poolingRemain; j++)
            {
                FD_SET((*playerIter)->Sock, &reads); // 남은 소켓을 FD_SET에 등록
                playerIter++;
            }
        }

        // 3. Select 처리
        int recvSelect = select(0, &reads, NULL, NULL, &select_Non_Blocking);

        if (recvSelect == SOCKET_ERROR)
        {
            SOCK_ERROR_PRINT("select - recv");
            DebugBreak();
        }

        // 소켓 처리 반복
        auto playerIter = playerList.begin();
        for (int i = 0; i < poolingCount * SELECT_MAX_SIZE + poolingRemain; ++i)
        {
            if (FD_ISSET((*playerIter)->Sock, &reads))
            {
                if ((*playerIter)->Alive == false)
                {
                    continue;
                }

                char Message[MESSAGE_BUFFER_SIZE] = { 0, };

                int recvByte = recv((*playerIter)->Sock, Message, MESSAGE_BUFFER_SIZE, 0);

                if (recvByte == SOCKET_ERROR)
                {
                    if (WSAGetLastError() == WSAEWOULDBLOCK)
                    {
                        continue;
                    }
                    else
                    {
                        (*playerIter)->Alive = false;
                        continue;
                    }
                }

                if (recvByte == 0)
                {
                    (*playerIter)->Alive = false;
                    continue;
                }

                (*playerIter)->recvBuffer->Enqueue(Message, recvByte);
            }

            playerIter++;

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

void Network::SessionAdvisor()
{

    auto deletePlayer = playerList.begin();


    while (deletePlayer != playerList.end())
    {
        if ((*deletePlayer)->Alive == false)
        {
            delete(*deletePlayer);
            deletePlayer = playerList.erase(deletePlayer);
        }
        else
        {
            deletePlayer++;
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
                if (it.second->mySession->SessoinID != session->SessoinID)
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
                if (Contents_Player[it]->mySession->SessoinID != session->SessoinID)
                {
                    NETWORK_UNICAST(sendPacket.GetBufferPtr(), Contents_Player[it]->mySession, &sendHeader);
                }
            }
        }

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