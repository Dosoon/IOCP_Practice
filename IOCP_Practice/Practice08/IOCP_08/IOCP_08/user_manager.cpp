#include "user_manager.h"

UserManager::~UserManager()
{
	for (auto& user : user_obj_pool_)
	{
		delete user;
	}
}

void UserManager::Init(const int32_t max_user_cnt)
{
	max_user_cnt_ = max_user_cnt;

	for (auto i = 0; i < max_user_cnt_; i++)
	{
		user_obj_pool_.push_back(new User());
		user_obj_pool_[i]->Init(i);
	}
}

User* UserManager::GetUserByIndex(int32_t user_index)
{
	if (user_index < 0 || user_index >= user_obj_pool_.size()) {
		return nullptr;
	}

	return user_obj_pool_[user_index];
}

void UserManager::DeleteUserInfo(const std::string& user_id)
{
	if (auto iter = user_id_map_.find(user_id); iter != user_id_map_.end())
	{
		user_id_map_.erase(iter);
	}
}

int32_t UserManager::FindUserIndexByID(const std::string& user_id)
{
	if (user_id_map_.contains(user_id)) {
		return user_id_map_[user_id];
	}

	return -1;
}

void UserManager::AddUser(const std::string& user_id, int32_t user_index)
{
	user_obj_pool_[user_index]->SetLogin(user_id);
	user_id_map_.insert({user_id, user_index});
}
