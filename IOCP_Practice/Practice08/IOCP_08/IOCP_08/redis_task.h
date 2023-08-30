#pragma once
#include "error_code.h"

#define kMAX_USER_ID_LEN 20
#define kMAX_USER_PW_LEN 20

enum class RedisTaskID : uint16_t
{
	kINVALID = 0,

	kREQUEST_LOGIN = 1001,
	kRESPONSE_LOGIN = 1002,
};

struct RedisTask
{
	uint32_t UserIndex = 0;
	RedisTaskID TaskID = RedisTaskID::kINVALID;
	uint16_t DataSize = 0;
	char* pData = nullptr;

	void Release()
	{
		if (pData != nullptr)
		{
			delete[] pData;
		}
	}
};

#pragma pack(push,1)

struct RedisLoginReq
{
	char UserID[kMAX_USER_ID_LEN + 1];
	char UserPW[kMAX_USER_PW_LEN + 1];
};

struct RedisLoginRes
{
	uint16_t Result = (uint16_t)ERROR_CODE::kNONE;
};

#pragma pack(pop)