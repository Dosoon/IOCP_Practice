#pragma once

struct Packet
{
	int32_t session_index_;
	int32_t data_size;
	char* data;

	Packet()
	{
		session_index_ = -1;
		data_size = 0;
		data = nullptr;
	}

	Packet(int32_t session_index, int32_t data_size, char* data)
		: session_index_(session_index), data_size(data_size), data(data)
	{
	}
};