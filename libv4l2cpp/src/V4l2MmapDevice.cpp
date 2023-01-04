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


void save_image_disk(std::queue<raw_ts> &raw_queue, FILE *record_file,  ofstream &record_infor, bool &need_record, int record_pack_size,ifstream  * record_file_dictt)
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
				// std::cout<<"raw_queue size " <<raw_queue.size()<<" after pop \n";

				// unsigned long t = buf.timestamp.tv_sec*1000+buf.timestamp.tv_usec/1000;  //  this time start from computer boot up
				record_info_struct tmp ;
				// unsigned long t = rts.t.tv_sec*1000+rts.t.tv_usec/1000;
				tmp.tm = rts.t;
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
			// if(record_file_dictt->is_open()){
			// 	record_file_dictt->close();
			// }
			// if(record_infor.is_open()){
				record_infor.close();
			// }
			fclose(record_file);
			
			 // thread exit when do not need record anymore.
			return;
		}
    }
}

void string_split(const string& str, const char split, vector<string>& res)
{
	if (str == "")        return;
	//在字符串末尾也加入分隔符，方便截取最后一段
	string strs = str + split;
	size_t pos = strs.find(split);
	// 若找不到内容则字符串搜索函数返回 npos
	while (pos != strs.npos)
	{
		string temp = strs.substr(0, pos);
		res.push_back(temp);
		//去掉已分割的字符串,在剩下的字符串中进行分割
		strs = strs.substr(pos + 1, strs.size());
		pos = strs.find(split);
	}
}

void string2map(const string& str, const char split, map<string,string>& res)
{
	//  res =  {"id":"123abc","status":"true"}     
	if (str == "")        return;
	//在字符串末尾也加入分隔符，方便截取最后一段
	string strs = str + split;
	size_t pos = strs.find(split);
	// 若找不到内容则字符串搜索函数返回 npos
	while (pos != strs.npos)
	{
		string temp = strs.substr(0, pos);
		int pos_temp = temp.find(":");
		string first_str = temp.substr(0,pos_temp);
		int left_p_f = first_str.find("\"");
		int right_p_f = first_str.rfind("\"");
		string scd_str = temp.substr(pos_temp,temp.size()-1);
		if(scd_str.find("\"") != string::npos  &&  scd_str.rfind("\"") != string::npos){
			int left_p_s= scd_str.find("\"");
			int right_p_s = scd_str.rfind("\"");
			// cout << "yes left_p_s:"<<left_p_s<<", right_p_s:"<<right_p_s<<endl;
			res.insert(make_pair(first_str.substr(left_p_f+1,right_p_f-left_p_f-1),scd_str.substr(left_p_s+1,right_p_s-left_p_s-1)));
		}else{
			int left_p_s= scd_str.find(":");
			int right_p_s = scd_str.rfind("}");
			// cout << " not left_p_s:"<<left_p_s<<", right_p_s:"<<right_p_s<<endl;
			res.insert(make_pair(first_str.substr(left_p_f+1,right_p_f-left_p_f-1),scd_str.substr(left_p_s+1,right_p_s-left_p_s-1)));
		}
		//去掉已分割的字符串,在剩下的字符串中进行分割
		strs = strs.substr(pos + 1, strs.size());
		pos = strs.find(split);
	}
}

string V4l2MmapDevice::find_file_by_id(string id){
	ifstream srcFile((record_path+"dict.info").c_str(), ios::in); 
	string result = "";
	if (srcFile.is_open())
	{
		string str;
		while (getline(srcFile,str))
		{
			int first = str.find(':');
			string id_ = str.substr(0,first);
			if(id.compare(id_)==0){
				string tm_devname =  str.substr(first+1,str.length()-first);;
				int second = tm_devname.find(':');
				result = tm_devname.substr(0,second);
				device_name = tm_devname.substr(second+1,tm_devname.length()-second);
				cout <<" id="<<id_<<", tm="<<result<<", device_name="<<device_name<<endl;
				return result;
			}
		}
	}
	else
	{
		cout << "find_file_by_id( "<<id<<") failed!" << endl;
	}
    srcFile.close();
	return "";
}
// std::string Time_t2String(time_t stamp) { 
//    tm* stamp_tm = localtime(&stamp);
//   std::ostringstream os;
//   os << std::put_time(stamp_tm, "%Y.%m.%d %H:%M:%S");
//   return os.str();
// }
// fstream  record_infor2;
bool V4l2MmapDevice::init(unsigned int mandatoryCapabilities)
{
	bool ret = V4l2Device::init(mandatoryCapabilities);
	if (ret)
	{
		// cap.open("test.264");
		signal(SIGINT, exit_sighandler);
        signal(SIGSEGV, exit_sighandler);
		encoder_ = new x264_encoder(m_width , m_height);
		redis_ = new Redis("tcp://ad@"+m_params.redis_server_ip+":6379");
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
		std::cout <<"\nRedis server connected! "<<m_params.m_devName<<","<<m_params.redis_server_ip<<"\n";
		int ttt = m_params.m_devName.rfind("/");
		device_name = m_params.m_devName.substr(ttt+1,m_params.m_devName.length()-ttt);
		
		
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
			// cout << record_path << " aleardy exist" << endl;
		}
        std::cout<< "ad_video_record_path: "<<record_path <<"\n";
		auto pack_size =  redis_->get("ad_video_record_pack_size");
		if(pack_size){
			record_pack_size = std::stoi(*path);
		}else{
			record_pack_size =1024*1024*1204;
		}
		std::cout<< "record_pack_size: "<<std::to_string(record_pack_size) <<"\n";

		std::thread replay_cali_time_thread = std::thread([this]() {
			try{
				auto sub = redis_->subscriber();
				sub.on_message([this](std::string channel, std::string msg) {
					// cout <<"appolo_record_calibration_time_pub = "<< msg<<endl;
					map<string,string> m;
					string2map(msg,',', m);
					if( m.end()!=m.find("calibration")  ){
						cali_time_n_str = m.find("calibration")->second;
						// cout << "cali_time_n_str ="<<cali_time_n_str<<endl;
						got_new_cali_time = true;
					}
				});
				sub.subscribe("apollo_record_calibration_time_pub");
				while (true) {
						sub.consume();
				}
			}catch (const Error &err) {
				std::cout <<"RedisHandler: sub config files error "  << err.what();
				return;
			}
		});
		replay_cali_time_thread.detach();
		
		std::thread video_record_thread = std::thread([this]() {
			try{
				auto sub = redis_->subscriber();
				sub.on_message([this](std::string channel, std::string msg) {
					
					map<string,string> m;
					string2map(msg,',', m);
					for (auto s : m){
						cout << s.first << " =  "<<s.second << endl;
					}
					cout << endl;
					if( m.end()!=m.find("status") && m.find("status")->second == "true"){
						string id = m.find("id")->second;
						need_record = true;
						struct timeval time;
						gettimeofday(&time, NULL);
						record_start_time = time.tv_sec*1000 + time.tv_usec/1000;
						string record_file_name = record_path+ to_string(record_start_time)  ;
						record_file=fopen((record_file_name+".264").c_str(),"wb");
						ofstream dict_file ((record_path+"dict.info").c_str(),ios::out|ios::app);
						string tmp = id+":"+to_string(record_start_time)+":"+device_name+"\n";
						dict_file<<tmp;
						dict_file.close();
						record_infor.open((record_file_name+".info").c_str(),ios::out|ios::app|ios::binary);

						std::thread th1(save_image_disk,std::ref(raw_queue),record_file,std::ref(record_infor),std::ref(need_record),record_pack_size ,record_file_dictt);
						th1.detach();
						packed_size =0;
						redis_->set("ad_record_video_fb",to_string(record_start_time) );
					}else{
						need_record = false;
						redis_->set("ad_record_video_fb","failed");
						delete encoder_;
						encoder_ = new x264_encoder(m_width , m_height);
					}
				});
				sub.subscribe("ad_record_video");
				while (true) {
						sub.consume();
				}
			}catch (const Error &err) {
				std::cout <<"RedisHandler: video_record_thread error "  << err.what();
				return;
			}
		});
		video_record_thread.detach();
		std::thread video_play_thread = std::thread([this]() {
			try{
					auto sub = redis_->subscriber();
					sub.on_message([this](std::string channel, std::string msg) {
						cout<< "\n play video msg : "<< msg <<endl;
						if(play_model){
							mtx_replay.lock();
							// cout << "check files open status in stop stage: "<< record_file_dictt->is_open()<<","<<cap.isOpened()<<endl;
							if(record_file_dictt->is_open()){
								record_file_dictt->close();
							}
							if(cap.isOpened()){
								cap.release();
							}
							play_model = false;
							mtx_replay.unlock();
							sleep(1);
						}
						
						// cout << "check me : "<< record_file_dictt->is_open()<<","<<cap.isOpened()<<endl;
						// if(cap && cap.isOpened()){
						// 	cap.release();
						// }
						// if(record_file_dictt && record_file_dictt->is_open()){
						// 	record_file_dictt->close();
						// }
						map<string,string> m;
						string2map(msg,',', m);
						if( m.end()!=m.find("status") &&   m.find("status")->second == "true" ){
							// sleep(3);
							mtx_replay.lock();
							string record_file_name_part = find_file_by_id(m.find("id")->second);
							string record_file_name = record_path + record_file_name_part+".264";
							string pcd_file_list =  record_path +"pcd_dict.info";
							record_file_dictt= new ifstream(record_path + record_file_name_part+".info",ios::in|ios::binary);
							pcd_file_dictt= new ifstream(pcd_file_list,ios::in);
							if(!record_file_dictt) {
								cout << " create dict file failed" <<endl;
							}
							cap.open(record_file_name);
							// cout << "check files open status in play stage: "<< record_file_dictt->is_open()<<","<<cap.isOpened()<<endl;
							if (!cap.isOpened()){
									cout<<"vedio file "<<record_file_name<<" not found"<<endl;
									redis_->set("ad_play_video_fb","vedio file "+record_file_name+" not found");
							}else{
								cout << "video file "<<record_file_name<<" start to replay"<<endl;
								redis_->set("ad_play_video_fb","vedio file "+record_file_name+" start to replay");
								int w = cap.get(CV_CAP_PROP_FRAME_WIDTH);
								int h = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
								cout<< "video width:"<<w<<", hight:"<<h<<endl;
								cap.set(CV_CAP_PROP_FRAME_WIDTH,w);
								cap.set(CV_CAP_PROP_FRAME_HEIGHT,h);
								play_model = true;
							}
							mtx_replay.unlock();
						}else if( m.end()!=m.find("status") && m.find("status")->second == "false"  ){
							mtx_replay.lock();
							play_model = false;
							// cout << "check files open status in stop stage: "<< record_file_dictt->is_open()<<","<<cap.isOpened()<<endl;
							if(record_file_dictt->is_open()){
								record_file_dictt->close();
							}
							if(cap.isOpened()){
								cap.release();
							}
							// cout<< " files closed";
							redis_->set("ad_play_video_fb",m.find("id")->second+" stoped");
							mtx_replay.unlock();
						}else if (m.end()!=m.find("status") &&   m.find("status")->second == "image"){
							string record_file_name_part = find_file_by_id(m.find("id")->second);
							string record_file_name = record_path + record_file_name_part+".264";
							string pcd_file_list =  record_path +"pcd_dict.info";
							record_file_dictt= new ifstream(record_path + record_file_name_part+".info",ios::in|ios::binary);
							pcd_file_dictt= new ifstream(pcd_file_list,ios::in);
							if(!record_file_dictt) {
								cout << " create dict file failed" <<endl;
							}
							cap.open(record_file_name);
							// cout << "check files open status in play stage: "<< record_file_dictt->is_open()<<","<<cap.isOpened()<<endl;
							if (!cap.isOpened()){
									cout<<"vedio file "<<record_file_name<<" not found"<<endl;
									redis_->set("ad_play_video_fb","vedio file "+record_file_name+" not found");
							}else{
								cout << "video file "<<record_file_name<<" start to replay"<<endl;
								redis_->set("ad_play_video_fb","vedio file "+record_file_name+" start to replay");
								int w = cap.get(CV_CAP_PROP_FRAME_WIDTH);
								int h = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
								cout<< "video width:"<<w<<", hight:"<<h<<endl;
								cap.set(CV_CAP_PROP_FRAME_WIDTH,w);
								cap.set(CV_CAP_PROP_FRAME_HEIGHT,h);
							}
							string str;
							string image_dir = record_path+device_name+"/";
							if (!boost::filesystem::is_directory(image_dir)) {
								cout << "begin create path: " << image_dir << endl;
								if (!boost::filesystem::create_directory(image_dir)) {
								cout << "create_directories failed: " << image_dir << endl;
								return -1;
								}
							} 
							while (getline(*pcd_file_dictt,str)){
								 stringstream strIn;
								strIn<<str;
								long long orig_pcd_ts_mili;  // as it is from micro-seconds
								strIn>>orig_pcd_ts_mili;
								long long pcd_ts_mili= orig_pcd_ts_mili/1000000;
								long long last_matched_pcd;

								while(true){
									cap >> frame;
									record_info_struct s; 
									if(record_file_dictt->read((char *)&s, sizeof(s))) { 
										// int readedBytes = record_file_dictt->gcount(); //看刚才读了多少字节
									}else{
										cout<<"read dict file error"<<endl;
									}
									long pic_ts_mili = s.tm.tv_sec*1000+s.tm.tv_usec/1000;
									long diff_mili = pcd_ts_mili - pic_ts_mili;
									// cout <<" read next pcd ts="<<pcd_ts_mili<<",img ts="<<pic_ts_mili<<", diff_mili="<<diff_mili<<endl;
									// cout <<" hi  pcd ts="<<pcd_ts_mili<<",img ts="<<pic_ts_mili<<", diff_mili="<<diff_mili<<endl;
									if(std::abs(diff_mili)<50 && ( last_matched_pcd != pcd_ts_mili)){
										cv::Point p ;
										p.x = m_width-420;
										p.y = 50;
										std::string time_str = Utils::Time_t2String( s.tm.tv_sec);
										time_str +="."+std::to_string((int)s.tm.tv_usec/1000);
										// cout <<time_str<<endl;
										cv::putText(frame, time_str, p, cv::FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 1, cv::LINE_AA);
										cout <<" < 50 pcd ts="<<pcd_ts_mili<<",img ts="<<pic_ts_mili<<", diff_mili="<<diff_mili<<endl;
										string img_name = image_dir+str+".jpg";
										cv::imwrite(img_name,	frame);
										last_matched_pcd = pcd_ts_mili;
										break;
									}else if(diff_mili < 0){   // pcd before , no picture match  . both pic and pcd is just forward
										cout << " * NOTICE :  PCD "<<str<< " no matched , neerliest pictrue(" << "diff is "<<diff_mili<< ") abord"<<endl;
										break;
									}else if (diff_mili > 0){  // skip some pictures to match
										// cout << "origianl diff_mili = "<<diff_mili<<endl;
										int skip_numb = diff_mili/33;
										long pic_ts_mili ;
										for(int i=0;i<=skip_numb;i++){  //  "=" means use the last pic to save
												cap >> frame;
												record_info_struct s; 
												if(record_file_dictt->read((char *)&s, sizeof(s))) { 
													// int readedBytes = record_file_dictt->gcount(); //看刚才读了多少字节
												}else{
													cout<<"read dict file error"<<endl;
												}
												 pic_ts_mili = s.tm.tv_sec*1000+s.tm.tv_usec/1000;
												// cout << " skipped "<< pic_ts_mili<<endl;
										}
										long diff_mili = pcd_ts_mili - pic_ts_mili;
										cv::Point p ;
										p.x = m_width-420;
										p.y = 50;
										std::string time_str = Utils::Time_t2String( s.tm.tv_sec);
										time_str +="."+std::to_string((int)s.tm.tv_usec/1000);
										// cout <<time_str<<endl;
										cv::putText(frame, time_str, p, cv::FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 1, cv::LINE_AA);
										cout <<"after skipped pcd ts="<<pcd_ts_mili<<",img ts="<<pic_ts_mili<<", diff_mili="<< diff_mili<<endl;
										string img_name = image_dir+str+".jpg";
										cv::imwrite(img_name,	frame);
										break;
									}

								}
							}
							
			// cv::imwrite("name",frame);
						}
					});
					sub.subscribe("ad_play_video");
					while (true) {
						sub.consume();
					}
				}catch (const Error &err) {
				std::cout <<"RedisHandler:video_play_thread error "  << err.what();
				return;
			}
		});
		video_play_thread.detach();

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
	if(record_file_dictt->is_open()){
		record_file_dictt->close();
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


void MatToData(cv::Mat srcImg, void*& data)
{
	int nFlag = srcImg.channels() * 8;//一个像素的bits
	int nHeight = srcImg.rows;
	int nWidth = srcImg.cols;
 
	int nBytes = nHeight * nWidth * nFlag / 8;//图像总的字节
	if (data)
		delete[] data;
	data = new unsigned char[nBytes];//new的单位为字节
	memcpy(data, srcImg.data, nBytes);//转化函数,注意Mat的data成员	
}



// FILE *jpg_file;

size_t V4l2MmapDevice::readInternal(char* buffer, size_t bufferSize)
{
	size_t size = 0;
	if(play_model){
		
		mtx_replay.lock();
		cap >> frame;
		record_info_struct s; 
		if(record_file_dictt->read((char *)&s, sizeof(s))) { 
            // int readedBytes = record_file_dictt->gcount(); //看刚才读了多少字节
        }else{
			cout<<"read dict file error"<<endl;
		}
		
		cv::Point p ;
		p.x = m_width-420;
		p.y = 50;
		std::string time_str = Utils::Time_t2String( s.tm.tv_sec);
		time_str +="."+std::to_string((int)s.tm.tv_usec/1000);
		// cout <<time_str<<endl;
		cv::putText(frame, time_str, p, cv::FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 1, cv::LINE_AA);
		
		std::vector <unsigned char> img_data;
		try{
			cv::imencode(".jpg", frame, img_data);
		}catch(cv::Exception ex){
			cout << " encode error exist replay model"<<endl;
			play_model = false;
		}
		if (!img_data.empty())  
		{  
			memcpy(buffer, &img_data[0], img_data.size()*sizeof(img_data[0]));  
		} else{
			cout << "frame is empty "<<endl;
			play_model = false;
		}
		size =img_data.size() ;
		// cout << "size = "<<size;
		auto this_pic_time = std::chrono::system_clock::now(); 
		
		// cout << "hi "<<got_new_cali_time<< endl;
		if(got_new_cali_time ){
			stringstream ss ;
			long long ll_compare_time_mili ;
			ss << cali_time_n_str;
			ss >> ll_compare_time_mili; 
			ll_compare_time_mili = ll_compare_time_mili/1000000;  //   nanosecond -> millisecond   ： mili (hao),micro(wei),nana(na)
			long long temp_diff = s.tm.tv_sec*1000 +s.tm.tv_usec/1000 - ll_compare_time_mili;   // pic faster than point could.
			cout << "camera time stamp : "<< s.tm.tv_sec*1000 +s.tm.tv_usec/1000 << " ;  lidar time stamp:"<<ll_compare_time_mili<<endl;
			if( std::abs(temp_diff) > 150 ){   // point cloud 100 + picture 30
				if(temp_diff > 0){ //  camera fast
					cout<< "camera is faster than lidar " << temp_diff <<" mili seconds, sleep to wait"<< endl;
					usleep(temp_diff*1000);
					cout<<"wake up to continue now"<<endl;
				}else{
					int skip_frame_num = -temp_diff/33;
					cout<< "camera slow " << -temp_diff <<" mili seconds ,  we need skip "<<skip_frame_num<< " frames"<< endl;
					for(int i =0;i<skip_frame_num;i++){
						cap >> frame;
						if(record_file_dictt->read((char *)&s, sizeof(s))) { 
							// int readedBytes = record_file_dictt->gcount(); //看刚才读了多少字节
						}else{
							cout<<"read dict file error in skip loop"<<endl;
						}
					}
					cout << skip_frame_num<< " frames skipped , ready to continue"<<endl;
				} 
			}
			got_new_cali_time = false;
		}
		// cout << "hi"<<endl;
		auto duration =std::chrono::duration_cast<std::chrono::milliseconds>(this_pic_time - last_pic_time).count();
		// cout << "hi 2"<<endl;
		// std::cout << "decode image time:" << duration << std::endl;
		// duration = duration - just_time;

		// cout << "after ajusted " <<duration<<endl;
		// if(duration<0){

		// }else 
		if(duration<33 && duration >0){
			// cv::waitKey(  (++frames_video%30 ==0?34:33)-duration);
			// cout << " sleep  "<< duration<<endl;
			usleep(    ((++frames_video%30 ==0?34:33)-duration)*1000  );
		}
		mtx_replay.unlock();
		last_pic_time = std::chrono::system_clock::now();
		return size ;
	}else if (n_buffers > 0){
		// cout<<"\n why2 \n";
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
			if(need_record){
				int h_size = encoder_->encode_frame((unsigned char *)m_buffer[buf.index].start);
				// std::cout<<"x264 size : "<<h_size<<"\n";
				if(raw_queue.size()>pre_record_seconds){
					raw_ts temp = raw_queue.front();
					if(temp.prt){
						free( temp.prt);
					}
					raw_queue.pop();
				}
				unsigned char *zip_addr_=(unsigned char *)malloc(m_height*m_width*3);
				memcpy(zip_addr_, encoder_->encoded_frame, h_size);
				raw_queue.push( { zip_addr_,h_size,time_});
			}
			// std::cout<<"raw_queue size "<<raw_queue.size()<<"\n";



			auto end = std::chrono::system_clock::now();
			auto duration =
				std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
					.count();
			// std::cout << "capture image time:" << duration << std::endl;

			memcpy(buffer, jpg_p, ret);
			free(jpg_p);
			// fwrite(jpg_p, ret,1,h264_fp);
			// char jpg_file_name[100]; /*存放JPG图片名称*/
			// std::cout<<"sec:"<<buf.timestamp.tv_sec<<", usec:"<<buf.timestamp.tv_usec <<"\n";
			// sprintf(jpg_file_name,"%d.jpg",buf.timestamp.tv_usec );
			// jpg_file=fopen(jpg_file_name,"wb");
			//  fwrite(jpg_p,1,ret,jpg_file);
			//  fclose(jpg_file);
			size = ret;

			
/*
			
			h_size = encoder_->encode_frame((unsigned char *)m_buffer[buf.index].start);
			int truncateBytes = 0;
			if (h_size >= 4 &&
				encoder_->encoded_frame[0] == 0 &&
				encoder_->encoded_frame[1] == 0 &&
				encoder_->encoded_frame[2] == 0 &&
				encoder_->encoded_frame[3] == 1)
			{
				truncateBytes = 4;
			}
			else if (h_size >= 3 &&
				encoder_->encoded_frame[0] == 0 &&
				encoder_->encoded_frame[1] == 0 &&
				encoder_->encoded_frame[2] == 1)
			{
				truncateBytes = 3;
			}
			// printf("*************** truncateBytes = %d \n",truncateBytes);

			u_int8_t* newFrameDataStart = (u_int8_t*)(encoder_->encoded_frame + truncateBytes);
			h_size = h_size -truncateBytes;
			// unsigned char * tttt = (unsigned char *)malloc(h_size);
			// memcpy(tttt, newFrameDataStart, h_size);

			u_int8_t nal_unit_type = newFrameDataStart[0] & 31;
			// std::cout << "sent NALU type " << (int)nal_unit_type << " (" << h_size << ")" << std::endl;
			std::cout <<"nal_unit_type:"<<(int)nal_unit_type <<"\n";
			if (nal_unit_type == 8) // PPS
			{
				std::cout << "PPS seen\n";
			}
			else if (nal_unit_type == 7) // SPS
			{
				std::cout  << "SPS seen; siz\n";
			}
			else if (nal_unit_type == 5)
			{
				std::cout <<" I  frame \n";
			}else{
				std::cout <<"nal_unit_type:"<<nal_unit_type <<"\n";
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
