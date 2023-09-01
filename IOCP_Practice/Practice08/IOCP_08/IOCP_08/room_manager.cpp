#include "room_manager.h"

void RoomManager::Init(const int32_t begin_room_num, const int32_t max_room_cnt, const int32_t max_user_cnt)
{
}

void RoomManager::SetSendPacket(std::function<void(uint32_t, char*, uint16_t)> send_packet)
{
}

Room* RoomManager::GetRoomByIdx(const int32_t room_idx)
{
	return nullptr;
}

ERROR_CODE RoomManager::EnterRoom(User* p_user, const int32_t room_num)
{
	return ERROR_CODE();
}

ERROR_CODE RoomManager::LeaveRoom(User* p_user, const int32_t room_num)
{
	return ERROR_CODE();
}
