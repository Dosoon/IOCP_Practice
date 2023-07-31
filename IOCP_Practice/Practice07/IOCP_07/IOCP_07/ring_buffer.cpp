#include "ring_buffer.h"

#define DEFAULT_BUFFER_SIZE 15000

RingBuffer::RingBuffer()
{
	buf_ = new char[DEFAULT_BUFFER_SIZE];
	front_ = 0;
	rear_ = 0;
	size_ = DEFAULT_BUFFER_SIZE;
}

RingBuffer::RingBuffer(int32_t size)
{
	buf_ = new char[size];
	front_ = 0;
	rear_ = 0;
	size_ = size;
}

RingBuffer::~RingBuffer()
{
	delete[] buf_;
}

int32_t RingBuffer::GetBufferSize()
{
	return size_;
}

int32_t RingBuffer::GetSizeInUse()
{
	if (front_ > rear_)
		return (size_ - (front_ - rear_));
	return (rear_ - front_);
}

int32_t RingBuffer::GetFreeSize()
{
	return size_ - GetSizeInUse() - 1;
}

int32_t RingBuffer::GetContinuousEnqueueSize()
{
	if (front_ == size_ - 1 && front_ == rear_)
	{
		front_ = 0;
		rear_ = 0;
		return size_;
	}
	if ((rear_ + 1) % size_ == front_)
		return 0;
	if (front_ > rear_)
		return (front_ - rear_);
	if (rear_ == size_ - 1)
		return 1;
	return (size_ - rear_ - 1);
}

int32_t RingBuffer::GetContinuousDequeueSize()
{
	if (front_ == rear_)
		return 0;
	if (front_ == size_ - 1)
		return 1;
	if (front_ > rear_)
		return (size_ - front_);
	return (rear_ - front_);
}

int32_t RingBuffer::Enqueue(char* src, int32_t len)
{
	if (GetFreeSize() == 0)
		return 0;
	int32_t oldRear = rear_;
	for (int32_t iCnt = 0; iCnt < len; iCnt++)
	{
		if ((rear_ + 1) % size_ == front_)
			break;
		buf_[rear_] = *src;
		rear_ = (rear_ + 1) % size_;
		src++;
	}
	return rear_ - oldRear;
}

int32_t RingBuffer::Dequeue(char* dest, int32_t len)
{
	if (GetSizeInUse() == 0)
		return 0;
	int32_t oldFront = front_;
	for (int32_t iCnt = 0; iCnt < len; iCnt++)
	{
		if (front_ == rear_)
			break;
		*dest = buf_[front_];
		front_ = (front_ + 1) % size_;
		dest++;
	}
	return front_ - oldFront;
}

int32_t RingBuffer::Peek(char* dest, int32_t len)
{
	int32_t value = 0;
	int32_t tmp = front_;
	for (int32_t iCnt = 0; iCnt < len; iCnt++)
	{
		if (tmp == rear_)
			break;
		*dest = buf_[tmp];
		tmp = (tmp + 1) % size_;
		dest++;
		value++;
	}
	return value;
}

void RingBuffer::MoveRear(int32_t size)
{
	rear_ = (rear_ + size) % size_;
}

void RingBuffer::MoveFront(int32_t size)
{
	int32_t* p = nullptr;
	if (front_ < 0)
		*p = 1;
	front_ = (front_ + size) % size_;
	if (front_ < 0)
		*p = 1;
}

char* RingBuffer::GetBufferPtr()
{
	return buf_;
}

char* RingBuffer::GetFrontBufferPtr()
{
	return buf_ + (front_ % size_);
}

char* RingBuffer::GetRearBufferPtr()
{
	return buf_ + (rear_ % size_);
}

void RingBuffer::ClearBuffer()
{
	front_ = 0;
	rear_ = 0;
}