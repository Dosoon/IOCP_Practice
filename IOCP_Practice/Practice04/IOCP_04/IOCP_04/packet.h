#pragma once

struct Packet
{
	int32_t session_index_;
	int32_t data_size;
	char* data;
};