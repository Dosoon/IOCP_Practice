#include "room.h"

#include "constants.h"
#include "packet_generator.h"
#include "packet_id.h"

#include <Windows.h>
#include <iostream>

void Room::Init(int32_t room_idx, int32_t max_user_cnt)
{
	room_idx_ = room_idx;
	max_user_cnt_ = max_user_cnt;
}

void Room::SetSendPacket(std::function<void(uint32_t, char*, uint16_t)> send_packet)
{
	SendPacketFunc = send_packet;
}

ERROR_CODE Room::EnterUser(User* p_user)
{
	if (user_list_.size() >= max_user_cnt_)
	{
		return ERROR_CODE::kENTER_ROOM_FULL_USER;
	}

	user_list_.push_back(p_user);
	++current_user_cnt_;

	p_user->EnterRoom(room_idx_);

	return ERROR_CODE::kNONE;
}

void Room::LeaveUser(User* p_leave_user)
{
	std::erase_if(user_list_, 
				  [leave_user_id = p_leave_user->GetUserID()](User* p_user) {
						return leave_user_id == p_user->GetUserID();
				 });
}

void Room::NotifyChat(int32_t user_idx, const char* p_user_id, const char* p_msg)
{
	auto notify_pkt = SetPacketIdAndLen<ROOM_CHAT_NOTIFY_PACKET>(PACKET_ID::kROOM_CHAT_NOTIFY);
	CopyMemory(notify_pkt.user_id_, p_user_id, (kMAX_USER_ID_LEN + 1));
	CopyMemory(notify_pkt.msg_, p_msg, (kMAX_CHAT_MSG_SIZE + 1));

	BroadcastMsg(reinterpret_cast<char*>(&notify_pkt), sizeof(notify_pkt), user_idx, false);
}

void Room::BroadcastMsg(char* p_data, const uint16_t len, uint32_t pass_user_idx, bool except_me)
{
	for (auto& user : user_list_)
	{
		if (user == nullptr) {
			continue;
		}

		if (except_me && user->GetSessionIdx() == pass_user_idx) {
			continue;
		}

		SendPacketFunc(static_cast<uint32_t>(user->GetSessionIdx()), p_data, len);
	}
}
