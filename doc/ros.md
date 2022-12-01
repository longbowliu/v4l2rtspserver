
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

