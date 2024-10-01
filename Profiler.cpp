#include "Profiler.h"





ProfileManager& ProfileManager::GetInstance()
{
	static ProfileManager instance;
	return instance;
}

void ProfileManager::ProfileBegin(std::string tagName)
{
	LARGE_INTEGER startTime;
	QueryPerformanceCounter(&startTime);

	auto it = ProfileManager::GetInstance()._ProfileMap.find(tagName);


	if (it != _ProfileMap.end())
	{
		ProfileManager::GetInstance()._ProfileMap[tagName].st_StartTime = startTime;
		ProfileManager::GetInstance()._ProfileMap[tagName].st_Call++;
	}
	else
	{
		Profile_Data temp;					// 최초 한 번만 실행
		{
			temp.st_TagName = tagName;
			temp.st_StartTime = startTime;
			temp.st_TotalTime = 0;
			temp.st_Min[0] = LLONG_MAX;
			temp.st_Min[1] = LLONG_MAX;
			temp.st_Max[0] = 0;
			temp.st_Max[1] = 0;
			temp.st_Call = 1;
		}
		ProfileManager::GetInstance()._ProfileMap[tagName] = temp;
	}

}

int ProfileManager::ProfileEnd(std::string tagName)
{
	auto it = ProfileManager::GetInstance()._ProfileMap.find(tagName);

	if (it != _ProfileMap.end())
	{
		LARGE_INTEGER endTime;
		QueryPerformanceCounter(&endTime);

		__int64 Value = (endTime.QuadPart - ProfileManager::GetInstance()._ProfileMap[tagName].st_StartTime.QuadPart);
		ProfileManager::GetInstance()._ProfileMap[tagName].st_TotalTime += Value;


		//---------------------------------------------------------------------/
		//							최소 최대 값 측정
		//--------------------------------------------------------------------/

		// 1. 최소 값 측정
		if (Value < ProfileManager::GetInstance()._ProfileMap[tagName].st_Min[0])
		{
			ProfileManager::GetInstance()._ProfileMap[tagName].st_Min[1] = ProfileManager::GetInstance()._ProfileMap[tagName].st_Min[0];
			ProfileManager::GetInstance()._ProfileMap[tagName].st_Min[0] = Value;
		}

		///////////////////////////////////////////////////////////////////


		// 2. 최대 값 측정
		if (Value > ProfileManager::GetInstance()._ProfileMap[tagName].st_Max[0])
		{
			ProfileManager::GetInstance()._ProfileMap[tagName].st_Max[1] = ProfileManager::GetInstance()._ProfileMap[tagName].st_Max[0];
			ProfileManager::GetInstance()._ProfileMap[tagName].st_Max[0] = Value;
		}
	}
	else
	{
		printf("End 시점에 존재하지 않는 tagName\n");
		DebugBreak();
	}


	return ProfileManager::GetInstance()._ProfileMap[tagName].st_Call;
}


void ProfileManager::Log(std::string tagName)
{

	auto it = _ProfileMap.find(tagName);
	auto& Instance = ProfileManager::GetInstance();

	if (it == _ProfileMap.end())
	{
		printf("Find Faild");
		return;
	}

	std::cout << "Tick Unit / 1 Tick = 0.1 ms, 100ns ." << std::endl;
	std::cout << "Total Tick : " << Instance._ProfileMap[tagName].st_TotalTime << '\n';
	std::cout << "Total Call : " << Instance._ProfileMap[tagName].st_Call << '\n';
	std::cout << "Total Min  : " << Instance._ProfileMap[tagName].st_Min[0] << '\n';
	std::cout << "Total Max : " << Instance._ProfileMap[tagName].st_Max[0] << '\n';

	double fl = ProfileManager::GetInstance()._ProfileMap[tagName].st_TotalTime / ProfileManager::GetInstance()._ProfileMap[tagName].st_Call;
	printf("Total Average : %.4f", fl);

}

ProfileManager::~ProfileManager()
{
	FILE* pFile;

	errno_t failCheck = fopen_s(&pFile, "Profile Log.txt", "wb");

	if (failCheck != 0)
	{
		printf("fopen_s fail\n");
		return;
	}

	char buffer[] = "-------------------------------------------------------------------------------\n";
	fseek(pFile, 0, SEEK_SET);
	fwrite(buffer, 1, strlen(buffer), pFile);

	char pormat[] = "           Name  |     Average  |        Min   |        Max   |        Call |\n";
	fwrite(pormat, 1, strlen(pormat), pFile);
	fwrite(buffer, 1, strlen(buffer), pFile);

	for (const auto it : _ProfileMap)
	{
		char member[256];
		sprintf_s(member, sizeof(member), " %15s | %10.4f㎲ | %10.4f㎲ | %10.4f㎲ | %10d \n",
			it.second.st_TagName.c_str(),
			((double)it.second.st_TotalTime / (double)it.second.st_Call) * 10,
			it.second.st_Min[0] / (double)it.second.st_Call,
			it.second.st_Max[0] / (double)it.second.st_Call,
			it.second.st_Call);

		fwrite(member, 1, strlen(member), pFile);
	}


	fwrite(buffer, 1, strlen(pormat), pFile);

}