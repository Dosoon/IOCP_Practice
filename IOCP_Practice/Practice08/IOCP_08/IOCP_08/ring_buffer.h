#pragma once

#include <stdint.h>

class RingBuffer
{
public:
	RingBuffer();
	RingBuffer(int32_t buf_size);
	~RingBuffer();

	int32_t GetBufferSize();
	int32_t GetSizeInUse();
	int32_t GetFreeSize();
	int32_t GetContinuousEnqueueSize();
	int32_t GetContinuousDequeueSize();

	int32_t Enqueue(const char* src, int32_t len);
	int32_t Dequeue(char* dest, int32_t len);
	int32_t Peek(char* dest, int32_t len);

	void MoveRear(int32_t size);
	void MoveFront(int32_t size);
	char* GetBufferPtr();
	char* GetFrontBufferPtr();
	char* GetRearBufferPtr();
	void ClearBuffer();

private:
	char* buf_;
	int32_t front_, rear_;
	int32_t size_;
};