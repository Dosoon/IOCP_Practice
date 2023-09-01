#pragma once

#include "user.h"

#include <vector>
#include <optional>
#include <unordered_map>

class UserManager
{
public:
	~UserManager();

	void Init(const int32_t max_user_cnt);
	
	User* GetUserByIndex(int32_t user_index);
	void DeleteUserInfo(const std::string& user_id);
	int32_t FindUserIndexByID(const std::string& user_id);
	void AddUser(const std::string& user_id, int32_t user_index);
	void CompleteProcess(uint32_t user_index, uint16_t pkt_size);

	int32_t GetCurrentUserCnt() 
	{
		return current_user_cnt_;
	}

	int32_t GetMaxUserCnt()
	{
		return max_user_cnt_;
	}

private:
	int32_t max_user_cnt_ = 0;
	int32_t current_user_cnt_ = 0;

	std::vector<User*> user_obj_pool_;
	std::unordered_map<std::string, int> user_id_map_;
};

