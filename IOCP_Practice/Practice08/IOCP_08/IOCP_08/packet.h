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
	uint16_t PacketLength;
	uint16_t PacketId;
	uint8_t Type; //���࿩�� ��ȣȭ���� �� �Ӽ��� �˾Ƴ��� ��
};

const uint32_t PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);

//- �α��� ��û

struct LOGIN_REQUEST_PACKET : public PACKET_HEADER
{
	char UserID[kMAX_USER_ID_LEN + 1];
	char UserPW[kMAX_USER_PW_LEN + 1];
};
const size_t LOGIN_REQUEST_PACKET_SZIE = sizeof(LOGIN_REQUEST_PACKET);


struct LOGIN_RESPONSE_PACKET : public PACKET_HEADER
{
	uint16_t Result;
};



//- �뿡 ���� ��û
//const int MAX_ROOM_TITLE_SIZE = 32;
struct ROOM_ENTER_REQUEST_PACKET : public PACKET_HEADER
{
	int32_t RoomNumber;
};

struct ROOM_ENTER_RESPONSE_PACKET : public PACKET_HEADER
{
	int16_t Result;
	//char RivalUserID[MAX_USER_ID_LEN + 1] = { 0, };
};


//- �� ������ ��û
struct ROOM_LEAVE_REQUEST_PACKET : public PACKET_HEADER
{
};

struct ROOM_LEAVE_RESPONSE_PACKET : public PACKET_HEADER
{
	int16_t Result;
};



// �� ä��
struct ROOM_CHAT_REQUEST_PACKET : public PACKET_HEADER
{
	char Message[kMAX_CHAT_MSG_SIZE + 1] = { 0, };
};

struct ROOM_CHAT_RESPONSE_PACKET : public PACKET_HEADER
{
	int16_t Result;
};

struct ROOM_CHAT_NOTIFY_PACKET : public PACKET_HEADER
{
	char UserID[kMAX_USER_ID_LEN + 1] = { 0, };
	char Msg[kMAX_CHAT_MSG_SIZE + 1] = { 0, };
};
#pragma pack(pop) //���� ������ ��ŷ������ �����

