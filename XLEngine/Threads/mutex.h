#pragma once
#include "../types.h"

class Mutex
{
public:
	virtual ~Mutex() {};

	virtual s32 lock() = 0;
	virtual s32 unlock() = 0;

protected:
	Mutex() {};

public:
	//static factory function - creates the correct platform dependent version
	static Mutex* create();
};
