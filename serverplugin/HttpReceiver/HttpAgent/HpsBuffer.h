#ifndef _HPS_BUFFER_H_
#define _HPS_BUFFER_H_
#include <stdlib.h>

const int HPS_BUFFER_INIT_CAPACITY = 4096;

#define HPS_ALIGN(size)  (((size) + 2047) & ~2047) //SAP_ALIGN(size, 2048) 
class CHpsBuffer
{
public:
	CHpsBuffer(int capacity=HPS_BUFFER_INIT_CAPACITY):capacity_(capacity),loc_(0)
	{
		base_=(char *)malloc(capacity);
	}
	void add_capacity()
	{
		capacity_+=capacity_;
		base_=(char *)realloc(base_,capacity_);
	}
	void add_capacity(int nLeft)
	{
		capacity_+=nLeft;
		base_=(char *)realloc(base_,capacity_);
	}
	void SetTopZero(){*(base_+loc_) = '\0';}
	char * base() const {return base_;}
	char * top() const {return base_+loc_;}
	void inc_loc(unsigned int loc){loc_+=loc;}
	void reset_loc(int loc){loc_=loc;}
	unsigned int capacity() const {return capacity_-loc_;}
	unsigned int len() const{return loc_;};
	~CHpsBuffer(){free(base_);}
private:
	char *base_;
	unsigned int capacity_;
	unsigned int loc_;
};

#endif


