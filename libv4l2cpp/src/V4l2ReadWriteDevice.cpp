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
#include <boost/filesystem.hpp>
#include <thread>

using namespace std;
using namespace utils_ns;
V4l2ReadWriteDevice::V4l2ReadWriteDevice(const V4L2DeviceParameters&  params, v4l2_buf_type deviceType) : V4l2Device(params, deviceType) {
}

size_t V4l2ReadWriteDevice::writeInternal(char* buffer, size_t bufferSize) { 
	return ::write(m_fd, buffer,  bufferSize); 
}
int index_ = 0;
int frames_total_  = 0;
bool finished = false;
auto last_pic_time = std::chrono::system_clock::now(); 
bool readable = false;
size_t V4l2ReadWriteDevice::readInternal(char* buffer, size_t bufferSize)  { 
		mtx_replay.lock();
		readable = cap.read(frame);
		mtx_replay.unlock();
		size_t size = 0;
		if(!readable){
			return size;
		}
		if(record_file_dictt && record_file_dictt->is_open()){
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
			cv::putText(frame, time_str, p, cv::FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 1, cv::LINE_AA);
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
		}
		std::vector <unsigned char> img_data;
		try{
			cv::imencode(".jpg", frame, img_data);
		}catch(cv::Exception ex){
			cout << "opencv encode error replay model"<<endl;
			return size;
		}
		// cv::imshow("test",frame);
		// if (cv::waitKey(30) == 27 )// 空格暂停
		// 	{
		// 		cv::waitKey(0);
		// 		// break;
		// 	}
		if (!img_data.empty())  
		{  
			memcpy(buffer, &img_data[0], img_data.size()*sizeof(img_data[0]));  
		} else{
			cout << "Note,  frame is empty! "<<endl;
			return size;
		}
		size =img_data.size() ;
		auto this_pic_time = std::chrono::system_clock::now(); 
		auto duration =std::chrono::duration_cast<std::chrono::milliseconds>(this_pic_time - last_pic_time).count();
		if(duration<33 && duration >0){
			usleep(    ((++frames_video%30 ==0?34:33)-duration)*1000  );
		}
		last_pic_time = std::chrono::system_clock::now(); 
		return size;
}

bool V4l2ReadWriteDevice::init(unsigned int mandatoryCapabilities)
{

bool ret = V4l2Device::init(mandatoryCapabilities);
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
			std::thread replay_cali_time_thread = std::thread([this]() {
			try{
				auto sub = redis_->subscriber();
				sub.on_message([this](std::string channel, std::string msg) {
					map<string,string> m;
					Utils::string2map(msg,',', m);
					if( m.end()!=m.find("calibration")  ){
						cali_time_n_str = m.find("calibration")->second;
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

		std::thread video_play_thread = std::thread([this]() {
			try{
					auto sub = redis_->subscriber();
					sub.on_message([this](std::string channel, std::string msg) {
						cout<< "\n play video msg : "<< msg <<endl;
						if(true){
							mtx_replay.lock();
							// cout << "check files open status in stop stage: "<< record_file_dictt->is_open()<<","<<cap.isOpened()<<endl;
							if(record_file_dictt && record_file_dictt->is_open()){
								record_file_dictt->close();
							}
							if(  cap.isOpened()){
								cap.release();
							}
							// play_model = false;
							mtx_replay.unlock();
							// sleep(1);
						}
						
						// cout << "check me : "<< record_file_dictt->is_open()<<","<<cap.isOpened()<<endl;
						// if(cap && cap.isOpened()){
						// 	cap.release();
						// }
						// if(record_file_dictt && record_file_dictt->is_open()){
						// 	record_file_dictt->close();
						// }
						map<string,string> m;
						Utils::string2map(msg,',', m);
						string  device_name ="" ; 
						if( m.end()!=m.find("status") &&   m.find("status")->second == "true" ){
							// sleep(3);
							mtx_replay.lock();
							string record_file_name_part = Utils::find_file_by_id(m.find("id")->second,record_path,device_name);
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
								// play_model = true;
							}
							mtx_replay.unlock();
						}else if( m.end()!=m.find("status") && m.find("status")->second == "false"  ){
							mtx_replay.lock();
							// play_model = false;
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
							string record_file_name_part = Utils::find_file_by_id(m.find("id")->second,record_path, device_name);
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
							string image_dir = record_path+ device_name+"/";
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

	m_format = V4L2_PIX_FMT_YUYV;
	record_file_name = m_params.m_devName;
	cap.open(record_file_name);
	if (!cap.isOpened()){
			cout<<"vedio file "<<record_file_name<<" not found"<<endl;
			redis_->set("ad_play_video_fb","vedio file "+record_file_name+" not found");
	}else{
		cout << "video file "<<record_file_name<<" start to replay"<<endl;
		redis_->set("ad_play_video_fb","vedio file "+record_file_name+" start to replay");
		int w = cap.get(CV_CAP_PROP_FRAME_WIDTH);
		int h = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
		cout<< "video width:"<<w<<", hight:"<<h<<endl;
		m_width      = w;
		m_height     = h;	
		m_bufferSize = 4147200;
	}
	return ret;
}
		
	

