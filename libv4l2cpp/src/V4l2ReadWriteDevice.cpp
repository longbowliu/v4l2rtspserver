/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** V4l2ReadWriteDevice.cpp
** 
** V4L2 source using read/write API
**
** -------------------------------------------------------------------------*/

#include <unistd.h>

#include "V4l2ReadWriteDevice.h"

using namespace std;

V4l2ReadWriteDevice::V4l2ReadWriteDevice(const V4L2DeviceParameters&  params, v4l2_buf_type deviceType) : V4l2Device(params, deviceType) {
}


size_t V4l2ReadWriteDevice::writeInternal(char* buffer, size_t bufferSize) { 
	return ::write(m_fd, buffer,  bufferSize); 
}

size_t V4l2ReadWriteDevice::readInternal(char* buffer, size_t bufferSize)  { 
	size_t size = 0;
	cap >> frame;
	std::vector <unsigned char> img_data;
	try{
		cv::imencode(".jpg", frame, img_data);
	}catch(cv::Exception ex){
		cout << " encode error exist replay model"<<endl;
		// play_model = false;
	}
	cv::imshow("test",frame);
	if (cv::waitKey(30) == 27 )// 空格暂停
		{
			cv::waitKey(0);
			// break;
		}
	if (!img_data.empty())  
	{  
		memcpy(buffer, &img_data[0], img_data.size()*sizeof(img_data[0]));  
	} else{
		cout << " why frame is empty "<<endl;
		// play_model = false;
	}
	size =img_data.size() ;

	return size;
}

bool V4l2ReadWriteDevice::init(unsigned int mandatoryCapabilities)
{
		/*   TODO :  V4l2Device.cpp  line 220
	m_format     = fmt.fmt.pix.pixelformat;
	m_width      = fmt.fmt.pix.width;
	m_height     = fmt.fmt.pix.height;	
	*/
	bool ret = V4l2Device::init(mandatoryCapabilities);
	m_format = V4L2_PIX_FMT_YUYV;
	record_file_name = "/home/demo/data/video_record/1671701583242.264";
	cap.open(record_file_name);
	if (!cap.isOpened()){
			cout<<"vedio file "<<record_file_name<<" not found"<<endl;
			// redis_->set("ad_play_video_fb","vedio file "+record_file_name+" not found");
	}else{
		cout << "video file "<<record_file_name<<" start to replay"<<endl;
		// redis_->set("ad_play_video_fb","vedio file "+record_file_name+" start to replay");
		int w = cap.get(CV_CAP_PROP_FRAME_WIDTH);
		int h = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
		cout<< "video width:"<<w<<", hight:"<<h<<endl;
		m_width      = w;
		m_height     = h;	
		m_bufferSize = 4147200;
		// cap.set(CV_CAP_PROP_FRAME_WIDTH,w);
		// cap.set(CV_CAP_PROP_FRAME_HEIGHT,h);
		// play_model = true;
	}
	return ret;
}
		
	

