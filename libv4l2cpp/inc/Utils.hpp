
#include <sys/stat.h>
#include <unistd.h>
#include <map>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include<iostream>
#include <algorithm>
#include <chrono>
#include <ctime>

using namespace std;

    


namespace utils_ns {

    class Utils{
        public:


            static string find_file_by_id(string id,string record_path, string &device_name){
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

            static string Time_t2String(time_t stamp) { 
                tm* stamp_tm = localtime(&stamp);
                std::ostringstream os;
                os << std::put_time(stamp_tm, "%Y.%m.%d %H:%M:%S");
                return os.str();
            }
            static void exit_sighandler(int sig)
            {
                std::cout<<"signal triggered , exist now ";
                sleep(2);
                exit(1);
            }

            static void string2map(const string& str, const char split, map<string,string>& res)
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
    };

}