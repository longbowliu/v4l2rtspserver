#include <iostream>
#include <opencv2/opencv.hpp>
using namespace std;
using namespace cv;
VideoCapture cap;
VideoWriter outputVideo;

int writevideo()
{
	cap.open(0);
	//cap.set(CAP_PROP_FRAME_WIDTH, 1024);
	//cap.set(CAP_PROP_FRAME_HEIGHT, 768);


	if (!cap.isOpened())
	{
		return -1;
	}
	int w = (int)cap.get(CV_CAP_PROP_FRAME_WIDTH);
	int h = (int)cap.get(CV_CAP_PROP_FRAME_HEIGHT);

	int frameRate = cap.get(CV_CAP_PROP_FPS);
	cout << w<<","<<h<<","<<frameRate << endl;
	int frameNum = 0;

	Mat frame, img;

	//保存视频的路径
	string outputVideoPath = "/home/demo/v4l2rtspserver/test222.mp4";
	outputVideo.open(outputVideoPath, CV_FOURCC('M', 'J', 'P', 'G'), 24.0, Size(640, 480));	//这是保存为avi的
	// outputVideo.open(outputVideoPath, CV_FOURCC('D', 'I', 'V', 'X'), 30.0, Size(640, 480));
	while (1)
	{
		cap >> frame;
		cv::Point p ;
		p.x = 10;
		p.y = 50;
		cv::putText(frame, "test", p, cv::FONT_HERSHEY_SIMPLEX, 1, (0, 255, 255), 2, cv::LINE_AA);
		resize(frame, frame, Size(640, 480));

		outputVideo.write(frame);


		frameNum += 1;
		cout << frameNum << endl;

		imshow("show", frame);
		//按键开始，暂停 	1000/25=40s 
		if (waitKey(30) == 27 )// 空格暂停
		{
			waitKey(0);
			break;
		}

	}
	outputVideo.release();
	return 0;
}

void readvideo(const string &filename)
{
	Mat frame;
	VideoCapture cap(filename);
	int frameNum = 0;
	while (1)
	{
		cap >> frame;
		frameNum += 1;
		cout << frameNum << endl;
		imshow("show", frame);
		if (waitKey(30) == 27 )// 空格暂停
		{
			waitKey(0);
			break;
		}


	}
	cap.release();
	destroyAllWindows();

}

int main()
{
	writevideo();
	// readvideo("test.264");
	

	return 0;
}

