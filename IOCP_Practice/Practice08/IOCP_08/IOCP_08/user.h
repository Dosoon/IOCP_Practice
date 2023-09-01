#pragma once

#include <stdint.h>
#include <string>
#include <optional>

#include "ring_buffer.h"
#include "packet.h"

class User
{
public:
	enum class DOMAIN_STATE {
		kNONE = 0,
		kLOGIN,
		kROOM
	};

	void Init(const int32_t session_idx)
	{
		session_idx_ = session_idx;
	}

	void Clear();
	void SetLogin(const std::string& user_id);
	void CompleteProcess(uint16_t pkt_size);
	void EnterRoom(int32_t room_idx);
	void LeaveRoom();

	// ------------------------------
	// Getter & Setter
	// ------------------------------

	int32_t GetSessionIdx()
	{
		return session_idx_;
	}

	void SetSessionIdx(const int32_t session_idx)
	{
		session_idx_ = session_idx;
	}

	int32_t GetRoomIdx()
	{
		return room_idx_;
	}

	void SetRoomIdx(const int32_t room_idx)
	{
		room_idx_ = room_idx;
	}

	std::string GetUserID()
	{
		return user_id_;
	}

	void SetUserID(const std::string& user_id)
	{
		user_id_ = user_id;
	}

	std::string GetAuthToken()
	{
		return auth_token_;
	}

	void SetAuthToken(const std::string& auth_token)
	{
		auth_token_ = auth_token;
	}

	bool IsConfirmed()
	{
		return is_confirmed_;
	}

	void SetConfirmed(const bool is_confirmed)
	{
		is_confirmed_ = is_confirmed;
	}

	DOMAIN_STATE GetDomainState()
	{
		return domain_state_;
	}

	void SetDomainState(const DOMAIN_STATE domain_state)
	{
		domain_state_ = domain_state;
	}

	// ------------------------------
	// Packet Buffer
	// ------------------------------

	std::optional<PacketInfo> GetPacketData();
	bool EnqueuePacketData(const char* src, int32_t len);

private:
	int32_t			session_idx_ = -1;
	int32_t			room_idx_ = -1;

	std::string		user_id_;
	std::string		auth_token_;

	bool			is_confirmed_ = false;

	DOMAIN_STATE	domain_state_ = DOMAIN_STATE::kNONE;

	RingBuffer		packet_buf_;
};

