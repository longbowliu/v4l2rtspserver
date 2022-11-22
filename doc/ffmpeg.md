
ffmpeg -i test.mp4 -r 30 -ss 00:00:20 -vframes 10 image-%3d.jpg
text.mp4视频文件
-r 30 每秒提取30帧，一般是24帧
image-%3d 文件命名格式是image-001.jpg
-ss，表示截取帧初始时间
-t，表示取t秒时间的帧   &&  -vframes，表示截取多少帧

ffmpeg -i test.264 -qscale:v 2  -ss 00:00:20 -vframes 5 -r 30   img-%3d.jpg
 	from test.264 file , start time 20, extract 5 frames .  30/fps
    	or use -t n extract n seconds frames.   
原文链接：https://blog.csdn.net/Gary__123456/article/details/89154095
