#pragma once
#include "constants.h"

struct PacketInfo
{
	uint32_t session_index_;
	uint16_t id_;
	uint16_t data_size_;
	char* data_;
};

#pragma pack(push,1)
struct PACKET_HEADER
{
	uint16_t packet_length_;
	uint16_t id_;
	uint8_t type_; //압축여부 암호화여부 등 속성을 알아내는 값
};

const uint32_t PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);

//- 로그인 요청

struct LOGIN_REQUEST_PACKET : public PACKET_HEADER
{
	char user_id_[kMAX_USER_ID_LEN + 1];
	char user_pw_[kMAX_USER_PW_LEN + 1];
};
const size_t LOGIN_REQUEST_PACKET_SZIE = sizeof(LOGIN_REQUEST_PACKET);


struct LOGIN_RESPONSE_PACKET : public PACKET_HEADER
{
	uint16_t result_;
};



//- 룸에 들어가기 요청
//const int MAX_ROOM_TITLE_SIZE = 32;
struct ROOM_ENTER_REQUEST_PACKET : public PACKET_HEADER
{
	int32_t room_number_;
};

struct ROOM_ENTER_RESPONSE_PACKET : public PACKET_HEADER
{
	int16_t result_;
	//char RivalUserID[MAX_USER_ID_LEN + 1] = { 0, };
};


//- 룸 나가기 요청
struct ROOM_LEAVE_REQUEST_PACKET : public PACKET_HEADER
{
};

struct ROOM_LEAVE_RESPONSE_PACKET : public PACKET_HEADER
{
	int16_t result_;
};



// 룸 채팅
struct ROOM_CHAT_REQUEST_PACKET : public PACKET_HEADER
{
	char msg_[kMAX_CHAT_MSG_SIZE + 1] = { 0, };
};

struct ROOM_CHAT_RESPONSE_PACKET : public PACKET_HEADER
{
	int16_t result_;
};

struct ROOM_CHAT_NOTIFY_PACKET : public PACKET_HEADER
{
	char user_id_[kMAX_USER_ID_LEN + 1] = { 0, };
	char msg_[kMAX_CHAT_MSG_SIZE + 1] = { 0, };
};
#pragma pack(pop) //위에 설정된 패킹설정이 사라짐

