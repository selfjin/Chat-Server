#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")


#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include "Network.h"

//-------------------------------------------------------------
//  define �� ���� ����
//-------------------------------------------------------------


// 1. ���� ON / OFF 

bool g_ShutDown = false;


int main()
{

	//---------------------------------------------------------------------/
	//							 �ʱ�ȭ �Լ�
	//--------------------------------------------------------------------/
	timeBeginPeriod(1);

	bool startUp = g_Server.WSASet();

	if (startUp == false)
	{
		std::cout << " startUp Fail " << '\n';
	}

	std::cout << "startUp Success " << '\n';
	
	// ---------------------------------------------------------------------/
	//							     Main
	// --------------------------------------------------------------------/

	int cnt = 12;

	while (!g_ShutDown)
	{
		g_Server.netIOProcess_LISTEN();

		g_Server.netIOProcess_RECV();

		g_Server.netIOProcess_SEND();

		g_Server.SessionAdvisor();

		Sleep(20);

		printf("%d\n", cnt++);
	}

}

