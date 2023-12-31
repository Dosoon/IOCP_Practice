#pragma once
#include "error_code.h"
#include "constants.h"


enum class REDIS_TASK_ID : uint16_t
{
	kINVALID = 0,

	kREQUEST_LOGIN = 1001,
	kRESPONSE_LOGIN = 1002,
};

struct RedisTask
{
	uint32_t UserIndex = 0;
	REDIS_TASK_ID TaskID = REDIS_TASK_ID::kINVALID;
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
	char user_id_[kMAX_USER_ID_LEN + 1];
	char user_pw_[kMAX_USER_PW_LEN + 1];
};

struct RedisLoginRes
{
	uint16_t result_ = (uint16_t)ERROR_CODE::kNONE;
};

#pragma pack(pop)