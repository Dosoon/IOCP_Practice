#pragma once

#include <cstdint>
#include <vector>
#include <functional>

#include "room.h"

class RoomManager
{
public:
	void Init(const int32_t begin_room_num, const int32_t max_room_cnt, const int32_t max_user_cnt);

	// ------------------------------
	// Getter & Setter
	// ------------------------------

	int32_t getMaxRoomCount()
	{
		return max_room_cnt_;
	}

	ERROR_CODE EnterRoom(User* p_user, const int32_t room_num);
	ERROR_CODE LeaveRoom(User* p_user, const int32_t room_num);

	Room* GetRoomByIdx(const int32_t room_idx);

	std::function<void(uint32_t, char*, uint16_t)> SendPacketFunc;

private:

	std::vector<Room*> room_list_;
	int32_t begin_room_num_ = 0;
	int32_t end_room_num_ = 0;
	int32_t max_room_cnt_;
};

