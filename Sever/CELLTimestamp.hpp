#ifndef _CELLTimestamp_hpp_
#define _CELLTimestamp_hpp_

#include <chrono>
using namespace std::chrono;
typedef long long ll;

class CELLTimestamp
{
public:
	CELLTimestamp()
	{
		update();
	}
	~CELLTimestamp()
	{

	}
	void update()
	{
		_begin = high_resolution_clock::now();
	}
	double getElapsedSecond()
	{
		return getElapsedTimeInMicroSec()*0.000001;
	}
	double getElapsedTimeInMilliSec()
	{
		return this->getElapsedTimeInMicroSec()*0.001;
	}
	ll getElapsedTimeInMicroSec()
	{
		auto t = duration_cast<microseconds>(high_resolution_clock::now() - _begin).count();
		return t;
	}
protected:
	time_point<high_resolution_clock> _begin;
};
#endif
