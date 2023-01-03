
从视频提取图片



ffmpeg -i test.264 -r 30 -ss 00:00:20 -vframes 10 image-%4d.jpg

text.mp4视频文件
-r 30 每秒提取30帧，一般是24帧
image-%3d 文件命名格式是image-001.jpg
-ss，表示截取帧初始时间
-t，表示取t秒时间的帧    
-vframes，表示截取多少帧

ffmpeg -i test.264 -qscale:v 2  -ss 00:00:20 -vframes 5 -r 30   img-%3d.jpg

ffmpeg -i test.264 -qscale:v 2  -r 30   img-%3d.jpg

 	from test.264 file , start time 20, extract 5 frames .  30/fps
    	or use -t n extract n seconds frames.   
原文链接：https://blog.csdn.net/Gary__123456/article/details/89154095


rtsp推流： 需要simple-rtsp-server 配合
ffmpeg -re -stream_loop -1 -i test.264 -r 30 -c copy -f rtsp rtsp://172.16.109.246:8554/h264ESVideoTest


从图片生成视频
ffmpeg -f image2 -i frame%4d.jpg test.mp4

ffmpeg -f image2 -i image-%4d.jpg test.mp4

其中，image_%d.jpg表示文件夹内的有序图片，比如image_00.jpg,image_01.jpg

使用ffmpeg将图片合成为视频(附完整参数介绍)
https://blog.csdn.net/xindoo/article/details/121451318

