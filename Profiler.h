
#pragma once
#include <iostream>
#include <map>
#include <Windows.h>

struct Profile_Data
{
	std::string st_TagName;

	LARGE_INTEGER st_StartTime;		// 프로파일 시작 값

	__int64 st_TotalTime;			// 전체 소요한 시간
	__int64 st_Min[2];				// 최소 사이 값 2개
	__int64 st_Max[2];				// 최대 사이 값 2개

	__int64 st_Call;				// 전체 콜 횟수
};

class ProfileManager
{
public:

	static ProfileManager& GetInstance();

	//---------------------------------------------------------------------/
	//							측정 관련 함수
	//--------------------------------------------------------------------/
	void ProfileBegin(std::string tagName);
	int ProfileEnd(std::string tagName);

	//---------------------------------------------------------------------/
	//						    출력 관련 함수
	//--------------------------------------------------------------------/
	void Log(std::string tagName);

	//---------------------------------------------------------------------/
	//						    파일 관련 함수
	//--------------------------------------------------------------------/

	~ProfileManager(); // 소멸자에서 파일화 처리
private:
	std::map<std::string, Profile_Data> _ProfileMap;
};

