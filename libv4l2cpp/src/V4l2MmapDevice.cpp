/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2MmapDevice.cpp
** 
** V4L2 source using mmap API
**
** -------------------------------------------------------------------------*/
#include <stdio.h>  // wtf
#include <jpeglib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h> 
#include <sys/mman.h>
#include <sys/ioctl.h>

// libv4l2
#include <linux/videodev2.h>

// project
#include "logger.h"
#include "V4l2MmapDevice.h"
#include <algorithm>
#include <chrono>
#include <signal.h>
#include <thread>
#include <sys/time.h>
#include <boost/filesystem.hpp>


// #include "../../src/x264_encoder.cpp"
// #include "yuv_to_jpg.cpp"





// FILE *h264_fp = fopen("/home/demo/INNO/repos/live/testProgs/test.264","wa+");

void exit_sighandler(int sig)
{
	std::cout<<"signal triggered , exist now ";
  	sleep(2);
	exit(1);
}

V4l2MmapDevice::V4l2MmapDevice(const V4L2DeviceParameters & params, v4l2_buf_type deviceType) : V4l2Device(params, deviceType), n_buffers(0) 
{
	
	memset(&m_buffer, 0, sizeof(m_buffer));
}


void save_image_disk(std::queue<raw_ts> &raw_queue, FILE *record_file,  ofstream &record_infor, bool &need_record, int record_pack_size)
{
    // FILE *h264_fp = fopen("/home/demo/test_buf_recorder123.h264","wa+");
    int packed_size = 0;
	
    while (true)
    {
        // std::unique_lock<std::mutex> lock(q_l);
        if ( need_record)
        {
			// cout<<" ************************** raw_queue size = "<<raw_queue.size()<<"\n";
			if(!raw_queue.empty() ){
				raw_ts rts = raw_queue.front();
				fwrite(rts.prt, rts.length,1,record_file);
				if(rts.prt){
					free(rts.prt);
				}
				raw_queue.pop();
				std::cout<<"raw_queue size " <<raw_queue.size()<<" after pop \n";

				// unsigned long t = buf.timestamp.tv_sec*1000+buf.timestamp.tv_usec/1000;  //  this time start from computer boot up
				record_info_struct tmp ;
				unsigned long t = rts.t.tv_sec*1000+rts.t.tv_usec/1000;
				tmp.tm = t;
				packed_size +=  rts.length;
				tmp.size= packed_size;
				record_infor.write((char *)&tmp,sizeof(tmp));
				record_infor.flush();
			}else{
				usleep(10000);
			}
			// cout <<"packed_size  record_pack_size"<<packed_size <<" ,"<<record_pack_size<<"\n";
			if(packed_size >record_pack_size){
				need_record = false;
			}
        }else{
			cout<<"exist from here \n";
			record_infor.close();
			fclose(record_file);
			 // thread exit when do not need record anymore.
			return;
		}
    }
}

// fstream  record_infor2;
bool V4l2MmapDevice::init(unsigned int mandatoryCapabilities)
{
	bool ret = V4l2Device::init(mandatoryCapabilities);
	if (ret)
	{
		signal(SIGINT, exit_sighandler);
        signal(SIGSEGV, exit_sighandler);
		encoder_ = new x264_encoder(m_width , m_height);
		redis_ = new Redis("tcp://city@172.16.109.246:6379");
		std::string ping_result ="";
		while (ping_result != "PONG"){
			try{
				ping_result = redis_->ping();
			}catch (const sw::redis::Error &err) {
				std::time_t t = std::time(NULL);
				std::tm *lctm = std::localtime(&t);
				if ( lctm->tm_sec<3){
					std::cout <<"\nSeem redis server not ready.  Detailed information: "  << err.what() << ",  waiting...\n";
				}
				usleep(2000000);
			}
		}
		std::cout <<"\nRedis server connected!\n";
		
		auto path = redis_->get("ad_video_record_path");
		if(path){
			record_path = *path;
		}else{
			record_path = "/home/demo/data/video_record/";
		}
		if (!boost::filesystem::is_directory(record_path)) {
			cout << "begin create path: " << record_path << endl;
			if (!boost::filesystem::create_directory(record_path)) {
			cout << "create_directories failed: " << record_path << endl;
			return -1;
			}
		} else {
			cout << record_path << " aleardy exist" << endl;
		}
        std::cout<< "ad_video_record_path: "<<record_path <<"\n";
		auto pack_size =  redis_->get("ad_video_record_pack_size");
		if(pack_size){
			record_pack_size = std::stoi(*path);
		}else{
			record_pack_size =1024*1024*1204;
		}
		std::cout<< "record_pack_size: "<<std::to_string(record_pack_size) <<"\n";

		std::thread sub_config_file_thread = std::thread([this]() {
			try{
				auto sub = redis_->subscriber();
				sub.on_message([this](std::string channel, std::string msg) {
							 if(msg == "true"){
								need_record = true;
								struct timeval time;
								gettimeofday(&time, NULL);
								record_start_time = time.tv_sec*1000 + time.tv_usec/1000;
								string record_file_name = record_path+ to_string(record_start_time)  ;
								record_file=fopen((record_file_name+".264").c_str(),"wb");
								// record_infor2.open((record_file_name+".infor2").c_str(),ios::out|ios::app);
								record_infor.open((record_file_name+".info").c_str(),ios::out|ios::app|ios::binary);
								std::thread th1(save_image_disk,std::ref(raw_queue),record_file,std::ref(record_infor),std::ref(need_record),record_pack_size);
								th1.detach();
								packed_size =0;
								redis_->set("ad_record_video_fb",to_string(record_start_time) );
							 }else{
								 need_record = false;
								 redis_->set("ad_record_video_fb","failed");
							 }
						});
				sub.subscribe("ad_record_video");
				while (true) {
						sub.consume();
				}
			}catch (const Error &err) {
				std::cout <<"RedisHandler: sub config files error "  << err.what();
				return;
			}
		});
		sub_config_file_thread.detach();
		ret = this->start();
	}
	return ret;
}

V4l2MmapDevice::~V4l2MmapDevice()
{
	delete encoder_;
	delete redis_;
	if(record_file){
		fclose(record_file);
	}
	if(record_infor.is_open()){
		record_infor.close();
	}
	this->stop();
}


bool V4l2MmapDevice::start() 
{
	LOG(NOTICE) << "Device " << m_params.m_devName;

	bool success = true;
	struct v4l2_requestbuffers req;
	memset (&req, 0, sizeof(req));
	req.count               = V4L2MMAP_NBBUFFER;
	req.type                = m_deviceType;
	req.memory              = V4L2_MEMORY_MMAP;

	if (-1 == ioctl(m_fd, VIDIOC_REQBUFS, &req)) 
	{
		if (EINVAL == errno) 
		{
			LOG(ERROR) << "Device " << m_params.m_devName << " does not support memory mapping";
			success = false;
		} 
		else 
		{
			perror("VIDIOC_REQBUFS");
			success = false;
		}
	}
	else
	{
		LOG(NOTICE) << "Device " << m_params.m_devName << " nb buffer:" << req.count;
		
		// allocate buffers
		memset(&m_buffer,0, sizeof(m_buffer));
		for (n_buffers = 0; n_buffers < req.count; ++n_buffers) 
		{
			struct v4l2_buffer buf;
			memset (&buf, 0, sizeof(buf));
			buf.type        = m_deviceType;
			buf.memory      = V4L2_MEMORY_MMAP;
			buf.index       = n_buffers;

			if (-1 == ioctl(m_fd, VIDIOC_QUERYBUF, &buf))
			{
				perror("VIDIOC_QUERYBUF");
				success = false;
			}
			else
			{
				LOG(INFO) << "Device " << m_params.m_devName << " buffer idx:" << n_buffers << " size:" << buf.length << " offset:" << buf.m.offset;
				m_buffer[n_buffers].length = buf.length;
				if (!m_buffer[n_buffers].length) {
					m_buffer[n_buffers].length = buf.bytesused;
				}
				m_buffer[n_buffers].start = mmap (   NULL /* start anywhere */, 
											m_buffer[n_buffers].length, 
											PROT_READ | PROT_WRITE /* required */, 
											MAP_SHARED /* recommended */, 
											m_fd, 
											buf.m.offset);

				if (MAP_FAILED == m_buffer[n_buffers].start)
				{
					perror("mmap");
					success = false;
				}
			}
		}

		// queue buffers
		for (unsigned int i = 0; i < n_buffers; ++i) 
		{
			struct v4l2_buffer buf;
			memset (&buf, 0, sizeof(buf));
			buf.type        = m_deviceType;
			buf.memory      = V4L2_MEMORY_MMAP;
			buf.index       = i;

			if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf))
			{
				perror("VIDIOC_QBUF");
				success = false;
			}
		}

		// start stream
		int type = m_deviceType;
		if (-1 == ioctl(m_fd, VIDIOC_STREAMON, &type))
		{
			perror("VIDIOC_STREAMON");
			success = false;
		}
	}
	return success; 
}

bool V4l2MmapDevice::stop() 
{
	LOG(NOTICE) << "Device " << m_params.m_devName;

	bool success = true;
	
	int type = m_deviceType;
	if (-1 == ioctl(m_fd, VIDIOC_STREAMOFF, &type))
	{
		perror("VIDIOC_STREAMOFF");      
		success = false;
	}

	for (unsigned int i = 0; i < n_buffers; ++i)
	{
		if (-1 == munmap (m_buffer[i].start, m_buffer[i].length))
		{
			perror("munmap");
			success = false;
		}
	}
	
	// free buffers
	struct v4l2_requestbuffers req;
	memset (&req, 0, sizeof(req));
	req.count               = 0;
	req.type                = m_deviceType;
	req.memory              = V4L2_MEMORY_MMAP;
	if (-1 == ioctl(m_fd, VIDIOC_REQBUFS, &req)) 
	{
		perror("VIDIOC_REQBUFS");
		success = false;
	}
	
	n_buffers = 0;
	return success; 
}
// FILE *jpg_file;
size_t V4l2MmapDevice::readInternal(char* buffer, size_t bufferSize)
{
	size_t size = 0;
	if (n_buffers > 0)
	{
		struct v4l2_buffer buf;	
		memset (&buf, 0, sizeof(buf));
		buf.type = m_deviceType;
		buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == ioctl(m_fd, VIDIOC_DQBUF, &buf)) 
		{
			perror("VIDIOC_DQBUF");
			size = -1;
		}
		else if (buf.index < n_buffers)
		{
			size = buf.bytesused;
			if (size > bufferSize)
			{
				size = bufferSize;
				LOG(WARN) << "Device " << m_params.m_devName << " buffer truncated available:" << bufferSize << " needed:" << buf.bytesused;
			}
			struct timeval time_;
			gettimeofday(&time_, NULL);

			auto start = std::chrono::system_clock::now();

		   unsigned char *jpg_p=(unsigned char *)malloc(m_height*m_width*3);

			int ret = yuv_to_jpeg(m_width,m_height,m_height*m_width*3,(unsigned char *)m_buffer[buf.index].start,jpg_p,80);
			// std::cout<<"jpeg size : "<<ret<<"\n";

		   	int h_size = encoder_->encode_frame((unsigned char *)m_buffer[buf.index].start);
			// std::cout<<"x264 size : "<<h_size<<"\n";

			if(raw_queue.size()>pre_record_seconds){
				raw_ts temp = raw_queue.front();
				if(temp.prt){
					free( temp.prt);
				}
				raw_queue.pop();
			}
			unsigned char *zip_addr_=(unsigned char *)malloc(h_size+1);
			memcpy(zip_addr_, encoder_->encoded_frame, h_size);
			raw_queue.push( { zip_addr_,h_size,time_});
			std::cout<<"raw_queue size "<<raw_queue.size()<<"\n";

		   if(0 && need_record && raw_queue.size()){
			raw_ts rts = raw_queue.front();

			fwrite(rts.prt, rts.length,1,record_file);
			// if(rts.prt){
			// 	free(rts.prt);
			// }
			raw_queue.pop();
			// unsigned long t = buf.timestamp.tv_sec*1000+buf.timestamp.tv_usec/1000;  //  this time start from computer boot up
			
			
			record_info_struct tmp ;
			unsigned long t = rts.t.tv_sec*1000+rts.t.tv_usec/1000;
			tmp.tm = t;
			packed_size += h_size;
			tmp.size= packed_size;
			record_infor.write((char *)&tmp,sizeof(tmp));
			record_infor.flush();
			// string infor = to_string(t) +"\t"+to_string(packed_size)+"\n";
			// record_infor2<<infor<<flush;
		   }

			auto end = std::chrono::system_clock::now();
			auto duration =
				std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
					.count();
			// std::cout << "capture image time:" << duration << std::endl;

			memcpy(buffer, jpg_p, ret);
			free(jpg_p);
			// fwrite(jpg_p, ret,1,h264_fp);
			// char jpg_file_name[100]; /*??????JPG????????????*/
			// std::cout<<"sec:"<<buf.timestamp.tv_sec<<", usec:"<<buf.timestamp.tv_usec <<"\n";
			// sprintf(jpg_file_name,"%d.jpg",buf.timestamp.tv_usec );
			// jpg_file=fopen(jpg_file_name,"wb");
			//  fwrite(jpg_p,1,ret,jpg_file);
			//  fclose(jpg_file);
			size = ret;

			

			/*
			size = encoder_->encode_frame((unsigned char *)m_buffer[buf.index].start);
			int truncateBytes = 0;
			if (size >= 4 &&
				encoder_->encoded_frame[0] == 0 &&
				encoder_->encoded_frame[1] == 0 &&
				encoder_->encoded_frame[2] == 0 &&
				encoder_->encoded_frame[3] == 1)
			{
				truncateBytes = 4;
			}
			else if (size >= 3 &&
				encoder_->encoded_frame[0] == 0 &&
				encoder_->encoded_frame[1] == 0 &&
				encoder_->encoded_frame[2] == 1)
			{
				truncateBytes = 3;
			}
			// printf("*************** truncateBytes = %d \n",truncateBytes);

			u_int8_t* newFrameDataStart = (u_int8_t*)(encoder_->encoded_frame + truncateBytes);
			size = size -truncateBytes;

			memcpy(buffer, newFrameDataStart, size);

			u_int8_t nal_unit_type = newFrameDataStart[0] & 0x1F;
			// std::cout << "sent NALU type " << (int)nal_unit_type << " (" << size << ")" << std::endl;

			if (nal_unit_type == 8) // PPS
			{
				std::cout << "PPS seen\n";
			}
			else if (nal_unit_type == 7) // SPS
			{
				std::cout  << "SPS seen; siz\n";
			}
			else
			{
				std::cout <<"nal_unit_type:"<< nal_unit_type <<"\n";
			}
			*/

			// fwrite(encoder_->encoded_frame, size,1,h264_fp);
			/*
			memcpy(buffer, m_buffer[buf.index].start, size);
			int encode_len = encoder_->encode_frame((unsigned char *)buffer);
			fwrite(encoder_->encoded_frame, encode_len,1,h264_fp);
			memcpy(buffer, encoder_->encoded_frame, encode_len);
			*/
			if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf))
			{
				perror("VIDIOC_QBUF");
				size = -1;
			}
		}
	}
	return size;
}

size_t V4l2MmapDevice::writeInternal(char* buffer, size_t bufferSize)
{
	size_t size = 0;
	if (n_buffers > 0)
	{
		struct v4l2_buffer buf;	
		memset (&buf, 0, sizeof(buf));
		buf.type = m_deviceType;
		buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == ioctl(m_fd, VIDIOC_DQBUF, &buf)) 
		{
			perror("VIDIOC_DQBUF");
			size = -1;
		}
		else if (buf.index < n_buffers)
		{
			size = bufferSize;
			if (size > buf.length)
			{
				LOG(WARN) << "Device " << m_params.m_devName << " buffer truncated available:" << buf.length << " needed:" << size;
				size = buf.length;
			}
			memcpy(m_buffer[buf.index].start, buffer, size);
			buf.bytesused = size;

			if (-1 == ioctl(m_fd, VIDIOC_QBUF, &buf))
			{
				perror("VIDIOC_QBUF");
				size = -1;
			}
		}
	}
	return size;
}

bool V4l2MmapDevice::startPartialWrite()
{
	if (n_buffers <= 0)
		return false;
	if (m_partialWriteInProgress)
		return false;
	memset(&m_partialWriteBuf, 0, sizeof(m_partialWriteBuf));
	m_partialWriteBuf.type = m_deviceType;
	m_partialWriteBuf.memory = V4L2_MEMORY_MMAP;
	if (-1 == ioctl(m_fd, VIDIOC_DQBUF, &m_partialWriteBuf))
	{
		perror("VIDIOC_DQBUF");
		return false;
	}
	m_partialWriteBuf.bytesused = 0;
	m_partialWriteInProgress = true;
	return true;
}

size_t V4l2MmapDevice::writePartialInternal(char* buffer, size_t bufferSize)
{
	size_t new_size = 0;
	size_t size = 0;
	if ((n_buffers > 0) && m_partialWriteInProgress)
	{
		if (m_partialWriteBuf.index < n_buffers)
		{
			new_size = m_partialWriteBuf.bytesused + bufferSize;
			if (new_size > m_partialWriteBuf.length)
			{
				LOG(WARN) << "Device " << m_params.m_devName << " buffer truncated available:" << m_partialWriteBuf.length << " needed:" << new_size;
				new_size = m_partialWriteBuf.length;
			}
			size = new_size - m_partialWriteBuf.bytesused;
			memcpy(&((char *)m_buffer[m_partialWriteBuf.index].start)[m_partialWriteBuf.bytesused], buffer, size);

			m_partialWriteBuf.bytesused += size;
		}
	}
	return size;
}

bool V4l2MmapDevice::endPartialWrite()
{
	if (!m_partialWriteInProgress)
		return false;
	if (n_buffers <= 0)
	{
		m_partialWriteInProgress = false; // abort partial write
		return true;
	}
	if (-1 == ioctl(m_fd, VIDIOC_QBUF, &m_partialWriteBuf))
	{
		perror("VIDIOC_QBUF");
		m_partialWriteInProgress = false; // abort partial write
		return true;
	}
	m_partialWriteInProgress = false;
	return true;
}
