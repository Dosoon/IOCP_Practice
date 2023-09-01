#include "room_manager.h"

#include "user.h"

RoomManager::~RoomManager()
{
	for (auto& room : room_list_)
	{
		delete room;
	}
}

void RoomManager::Init(const int32_t begin_room_num, const int32_t max_room_cnt, const int32_t max_user_cnt)
{
	begin_room_num_ = begin_room_num;
	max_room_cnt_ = max_room_cnt;
	
	for (auto i = 0; i < max_room_cnt_; i++)
	{
		room_list_.push_back(new Room());
		room_list_[i]->Init(i, max_user_cnt);
	}
}

void RoomManager::SetSendPacket(std::function<void(uint32_t, char*, uint16_t)> send_packet)
{
	SendPacketFunc = send_packet;
}

Room* RoomManager::GetRoomByIdx(const int32_t room_idx)
{
	if (room_idx < 0 || room_idx >= room_list_.size()) {
		return nullptr;
	}

	return room_list_[room_idx];
}

ERROR_CODE RoomManager::EnterRoom(User* p_user, const int32_t room_idx)
{
	auto p_room = GetRoomByIdx(room_idx);

	if (p_room == nullptr) {
		return ERROR_CODE::kROOM_INVALID_INDEX;
	}

	return p_room->EnterUser(p_user);
}

ERROR_CODE RoomManager::LeaveRoom(User* p_user, const int32_t room_idx)
{
	auto p_room = GetRoomByIdx(room_idx);

	if (p_room == nullptr) {
		return ERROR_CODE::kROOM_INVALID_INDEX;
	}

	p_room->LeaveUser(p_user);
	p_user->SetDomainState(User::DOMAIN_STATE::kLOGIN);

	return ERROR_CODE::kNONE;
}
