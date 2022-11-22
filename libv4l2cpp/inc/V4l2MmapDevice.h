/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2MmapDevice.h
** 
** V4L2 source using mmap API
**
** -------------------------------------------------------------------------*/


#ifndef V4L2_MMAP_DEVICE
#define V4L2_MMAP_DEVICE
 
#include "V4l2Device.h"
#include "../x264/inc/x264_encoder.h"
#include "../jpg/inc/yuv_to_jpg.h"
#include <../sw/redis++/redis++.h>
#include<fstream>
#include<iostream>
using namespace std;
#define V4L2MMAP_NBBUFFER 10
using namespace sw::redis;
class V4l2MmapDevice : public V4l2Device
{	
	protected:	
		size_t writeInternal(char* buffer, size_t bufferSize);
		bool   startPartialWrite();
		size_t writePartialInternal(char*, size_t);
		bool   endPartialWrite();
		size_t readInternal(char* buffer, size_t bufferSize);
			
	public:
		V4l2MmapDevice(const V4L2DeviceParameters & params, v4l2_buf_type deviceType);		
		virtual ~V4l2MmapDevice();

		virtual bool init(unsigned int mandatoryCapabilities);
		virtual bool isReady() { return  ((m_fd != -1)&& (n_buffers != 0)); }
		virtual bool start();
		virtual bool stop();
	private:
		 x264_encoder *encoder_;
		 Redis * redis_;
		 bool need_record = false;
		 unsigned long record_start_time=0 ;
		 std::string record_path ;
		 int record_pack_size;   // M 
		 unsigned long long packed_size;
		 
		 FILE *record_file; 
		 ofstream record_infor;
	
	protected:
		unsigned int  n_buffers;
	
		struct buffer 
		{
			void *                  start;
			size_t                  length;
		};
		buffer m_buffer[V4L2MMAP_NBBUFFER];

		struct record_info_struct
		{
			unsigned long tm;
			unsigned long long  size;
		};
};

#endif

