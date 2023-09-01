#pragma once

#include <cstdint>
#include <type_traits>

#include "redis_task.h"

// Packet에 대한 concept 정의
template <typename T>
concept Packet = requires(T t) {
	{ t.id_ } -> std::convertible_to<uint16_t>;
	{ t.packet_length_ } -> std::convertible_to<uint16_t>;
};

template <Packet Pkt>
Pkt SetPacketIdAndLen(PACKET_ID id)
{
	Pkt pkt;

	pkt.id_ = static_cast<uint16_t>(id);
	pkt.packet_length_ = sizeof(Pkt);

	return pkt;
};

template <typename TaskBody>
RedisTask SetTaskBody(uint32_t session_idx, REDIS_TASK_ID id, TaskBody body)
{
	RedisTask task;

	task.UserIndex = session_idx;
	task.TaskID = id;
	task.DataSize = sizeof(body);
	task.pData = new char[task.DataSize];
	CopyMemory(task.pData, reinterpret_cast<char*>(&body), task.DataSize);

	return task;
};