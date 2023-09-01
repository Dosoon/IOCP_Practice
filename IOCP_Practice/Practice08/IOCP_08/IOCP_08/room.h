#pragma once

#include <cstdint>
#include <vector>
#include <functional>

#include "user.h"
#include "error_code.h"

class Room
{
public:
	
	void Init(int32_t room_idx, int32_t max_user_cnt);

	// ------------------------------
	// Getter & Setter
	// ------------------------------
	int32_t GetRoomIdx()
	{
		return room_idx_;
	}

	void SetSendPacket(std::function<void(uint32_t, char*, uint16_t)> send_packet);

	// ------------------------------
	// Room ∑Œ¡˜
	// ------------------------------

	ERROR_CODE EnterUser(User* p_user);
	void LeaveUser(User* p_user);
	void NotifyChat(int32_t user_idx, const char* p_user_id, const char* p_msg);

private:
	// TODO : ¿≠¥‹ø°º≠ Bind«ÿ¡‡æﬂ «‘
	std::function<void(uint32_t, char*, uint16_t)> SendPacketFunc;

	void BroadcastMsg(char* p_data, const uint16_t len, uint32_t pass_user_idx, bool except_me);

	int32_t room_idx_ = -1;

	std::vector<User*> user_list_;

	int32_t max_user_cnt_;

	uint16_t current_user_cnt_ = 0;
};

