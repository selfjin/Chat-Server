#pragma once
#include "RingBuffer.h"
#include "Define.h"
#include "SerializationBuffer.h"
#include <unordered_map>
#include <list>
#include <Windows.h>



//-------------------------------------------------------------
//  NETWORK
//-------------------------------------------------------------

void SOCK_ERROR_PRINT(const char* string);

#define SERVER_PORT 6000
#define RINGBUFFER_SIZE 10000
#define MESSAGE_BUFFER_SIZE 10000
#define SELECT_MAX_SIZE 64

class Network;
extern Network g_Server;

///////////////////////////////////////////////////////////////

struct Session
{
	//-------------------------------------------------------------
	// Session »ý¼ºÀÚ ¹× ¼Ò¸êÀÚ
	//-------------------------------------------------------------
	Session(int bufferSize = RINGBUFFER_SIZE)
	{
		recvBuffer = new RingBuffer(bufferSize);
		sendBuffer = new RingBuffer(bufferSize);
	}
	~Session()
	{
		delete recvBuffer;
		delete sendBuffer;
	}

	//-------------------------------------------------------------
	// Network
	//-------------------------------------------------------------
	int Alive = true;
	SOCKET Sock;
	WCHAR Ip[16];
	int Port;
	int SessoinID;

	RingBuffer* recvBuffer;
	RingBuffer* sendBuffer;
};

struct Player
{
	Player(Session* session) : mySession(session)
	{
	}
	Session* mySession;
	WCHAR NickName[15];

	int RoomState = 0;
	bool loginState = false;
	bool roomVisited = false;
};



class Network
{
public:
	
	bool WSASet();
	bool Run();

	void NETWORK_UNICAST(char* packet, Session* user, PACKET_HEADER* header);
	void NETWORK_BROADCAST(char* packet, Session* exclude_Session, PACKET_HEADER* header);
	

	void NETWORK_PROC_ACCEPT();
	void netIOProcess_LISTEN();
	void netIOProcess_RECV();
	void netIOProcess_SEND();

	
	void netIoProcess_SelectRecvSend();

	void SessionAdvisor();


	void NETWORK_PROC(PACKET_HEADER* header, Session* session);
	void NET_PACKET_MP_HEADER(PACKET_HEADER* header, CPacket* payload, WORD msgType, WORD payLoadSize);


//-------------------------------------------------------------
//  ÄÁÅÙÃ÷ ÄÚµå µî·Ï
//-------------------------------------------------------------
	
	void (*PACKET_PROCEDURE_CALL)(PACKET_HEADER* header, Session* session) = nullptr;
	void (*CONTETNTS_PLAYER_ADVISOR_CALL)(int sessionID) = nullptr;

private:

	
	SOCKET Listen_Socket;
	SOCKADDR_IN serverAddr;
	
	int Session_ID_NUMBER = 0;
	std::list<Session*> playerList;

	fd_set _reads;
	fd_set _writes;

	timeval select_Non_Blocking;

};



