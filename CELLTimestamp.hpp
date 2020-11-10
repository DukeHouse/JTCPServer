﻿#pragma once
#ifndef _CELLTimestamp_HPP_
#define _CELLTimestamp_HPP_
#include <chrono>

class CELLTimestamp
{
public:
	CELLTimestamp()
	{
		this->update();
	}
	~CELLTimestamp()
	{

	}
	void update(){_begin = std::chrono::high_resolution_clock::now();}

	double getElapsedTimeInSec()
	{
		return this->getElapsedTimeInMircoSec() * 0.000001;
	}

	double getElapsedTimeInMilliSec()
	{
		return this->getElapsedTimeInMircoSec() * 0.001;
	}

	long long  getElapsedTimeInMircoSec()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - _begin).count();
	}

protected:
	std::chrono::time_point < std::chrono::high_resolution_clock> _begin;

};




#endif
