
rosbag使用
https://blog.csdn.net/lemonxiaoxiao/article/details/121693548

显示数据包中的信息:  
    rosbag info bag_name.bag --freq
        freq : 在topics字段中展示帧率信息

指定播放起始点
    rosbag play bag_name.bag -s 5 #从5s的地方开始播放

指定播放时长
    rosbag play bag_name.bag -u 250 #播放250s信息






从topic提取图片

rosrun image_view extract_images image:=/v4l2_camera/camera2/image  _image_transport:=compressed
rosrun image_view extract_images image:=/v4l2_camera/camera2/image  _image_transport:=compressed _sec_per_frame:=0.033

https://zhuanlan.zhihu.com/p/406079846




https://www.coder.work/article/7823657
另一种方法是解压缩数据然后保存。它是这样的:

$rosrun image_transport republish compressed in:=/v4l2_camera/camera2/image raw out:=image/raw

$rosrun image_view image_saver image:=image/raw _save_all_image:=all _filename_format:=%04d.%s

$ rosbag play <bagfile>



https://www.cnblogs.com/y-z-h/p/15918902.html

