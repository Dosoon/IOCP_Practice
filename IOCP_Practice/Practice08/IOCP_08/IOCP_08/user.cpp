#include "user.h"

#include "packet.h"

void User::Clear()
{
    room_idx_ = -1;
    user_id_ = "";
    is_confirmed_ = false;
    domain_state_ = DOMAIN_STATE::kNONE;

    packet_buf_.ClearBuffer();
}

void User::SetLogin(const std::string& user_id)
{
    domain_state_= DOMAIN_STATE::kLOGIN;
    user_id_ = user_id;
}

void User::CompleteProcess(uint16_t pkt_size)
{
    packet_buf_.MoveFront(pkt_size);
}

void User::EnterRoom(int32_t room_idx)
{
    room_idx_ = room_idx;
    domain_state_ = DOMAIN_STATE::kROOM;
}

void User::LeaveRoom()
{
    room_idx_ = -1;
    domain_state_ = DOMAIN_STATE::kLOGIN;
}

std::optional<PacketInfo> User::GetPacketData()
{
    PACKET_HEADER header;
    if (packet_buf_.Peek(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header)) {
        return {};
    }

    if (packet_buf_.GetSizeInUse() >= header.packet_length_) {

        return PacketInfo {
            .session_index_ = static_cast<uint32_t>(session_idx_),
            .id_ = header.id_,
            .data_size_ = header.packet_length_,
            .data_ = packet_buf_.GetFrontBufferPtr()
        };
    }

    return {};
}

bool User::EnqueuePacketData(const char* src, int32_t len)
{
    return packet_buf_.Enqueue(src, len) == len;
}
