#pragma once

#include <stdint.h>
#include <string>

#include "ring_buffer.h"

class User
{
public:
	enum class DOMAIN_STATE {
		NONE = 0,
		LOGIN,
		ROOM
	};

	bool EnqueuePacketData(char* src, int32_t len);

private:
	int32_t			session_idx_ = -1;
	int32_t			room_idx_ = -1;

	std::string		user_id_;
	std::string		auth_token_;

	DOMAIN_STATE	domain_state_ = DOMAIN_STATE::NONE;

	RingBuffer		pakcet_buf_;
};

