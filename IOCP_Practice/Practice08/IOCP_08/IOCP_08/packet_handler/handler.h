#pragma once

#include "../packet.h"

class BaseHandler
{
public:
	virtual bool Process(PacketInfo& packet) = 0;
};

class ConnectHandler : public BaseHandler
{
public:
	virtual bool Process(PacketInfo& packet) override;
};

class DisconnectHandler : public BaseHandler
{
public:
	virtual bool Process(PacketInfo& packet) override;
};

class LoginHandler : public BaseHandler
{
public:
	virtual bool Process(PacketInfo& packet) override;
};