#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")


#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include "Network.h"
#include "Contents.h"
#include "Profiler.h"
//-------------------------------------------------------------
//  define 및 전역 변수
//-------------------------------------------------------------


// 1. 서버 ON / OFF 

bool g_ShutDown = false;


int main()
{

	//---------------------------------------------------------------------/
	//							 초기화 함수
	//--------------------------------------------------------------------/
	timeBeginPeriod(1);

	bool startUp = g_Server.WSASet();

	if (startUp == false)
	{
		std::cout << " startUp Fail " << '\n';
	}

	std::cout << "startUp Success " << '\n';



	// 1. 네트워크 코드가 처리할 패킷 프로시저 등록

	g_Server.PACKET_PROCEDURE_CALL = NETWORK_PROC;
	g_Server.CONTETNTS_PLAYER_ADVISOR_CALL = CONTETNTS_PLAYER_ADVISOR;
	
	// ---------------------------------------------------------------------/
	//							     Main
	// --------------------------------------------------------------------/
	

	while (!g_ShutDown)
	{
		ProfileManager::GetInstance().ProfileBegin("Main");

		g_Server.netIOProcess_LISTEN();

		g_Server.netIoProcess_SelectRecvSend();
		
		g_Server.SessionAdvisor();

		ProfileManager::GetInstance().ProfileEnd("Main");

	}

	printf("End\n");

}

