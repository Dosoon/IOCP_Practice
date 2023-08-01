#pragma once

#include <thread>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "user_manager.h"

class PacketManager
{
public:
	PacketManager();
	~PacketManager();

	void SetUserManager(UserManager* user_manager)
	{
		user_manager_ = user_manager;
	}
	void EnqueuePacket(int32_t user_index);
	void EnqueueSystemPacket(const Packet& packet);

private:
	void CreatePacketProcessorThread();
	void ProcessPacket();
	void DestroyPacketProcessorThread();

	std::unordered_map<int32_t, void*> packet_procedure_by_code_;

	std::thread				packet_process_thread_;
	std::queue<int32_t>		incoming_packet_user_index_queue_;
	std::mutex				incoming_packet_lock_;
	std::queue<Packet>		system_packet_queue_;
	std::mutex				system_packet_lock_;

	UserManager*			user_manager_;
};