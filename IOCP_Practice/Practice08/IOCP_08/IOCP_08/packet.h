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
	uint8_t type_; //���࿩�� ��ȣȭ���� �� �Ӽ��� �˾Ƴ��� ��
};

const uint32_t PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);

//- �α��� ��û

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



//- �뿡 ���� ��û
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


//- �� ������ ��û
struct ROOM_LEAVE_REQUEST_PACKET : public PACKET_HEADER
{
};

struct ROOM_LEAVE_RESPONSE_PACKET : public PACKET_HEADER
{
	int16_t result_;
};



// �� ä��
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
#pragma pack(pop) //���� ������ ��ŷ������ �����

