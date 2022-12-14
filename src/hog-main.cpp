#include <ros/ros.h>
#include <iostream>

#include <opencv2/objdetect.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/synchronizer.h>
#include <message_filters/subscriber.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/Image.h>

using namespace cv;
using namespace std;
using namespace message_filters;

Mat rgb_frame, dpt_frame;

image_transport::Publisher img_pub;

cv_bridge::CvImagePtr rgb_ptr;
cv_bridge::CvImagePtr dpt_ptr;
cv_bridge::CvImagePtr cv_ptr(new cv_bridge::CvImage);

class Detector {
  HOGDescriptor hog;
public:
  Detector() : hog() {
    hog.setSVMDetector(HOGDescriptor::getDefaultPeopleDetector());
  }
  vector<Rect> detect(InputArray img) {
    vector<Rect> found;
    vector<double> weights;
    hog.detectMultiScale(img, found, weights, 0, Size(4,4), Size(8,8), 1.03, 2, true);
    return found;
  }
  void adjustRect(Rect & r) const {
    r.x += cvRound(r.width*0.1);
    r.width = cvRound(r.width*0.8);
    r.y += cvRound(r.height*0.07);
    r.height = cvRound(r.height*0.8);
  }
};

void RGBDcallback(const sensor_msgs::ImageConstPtr& msg_rgb , const sensor_msgs::ImageConstPtr& msg_depth) {  
  
  try { 
    rgb_ptr = cv_bridge::toCvCopy(*msg_rgb, sensor_msgs::image_encodings::BGR8); }
  catch (cv_bridge::Exception& e) {
    ROS_ERROR("Could not convert '%s' format", e.what()); }
 
  try{
    dpt_ptr = cv_bridge::toCvCopy(*msg_depth, sensor_msgs::image_encodings::TYPE_16UC1); }
  catch (cv_bridge::Exception& e) {
    ROS_ERROR("Could not convert '%s' format", e.what()); }

  rgb_frame = rgb_ptr->image;
  Detector detector; 
  vector<Rect> found = detector.detect(rgb_frame);

  for (vector<Rect>::iterator i = found.begin(); i != found.end(); ++i) {
    Rect &r = *i;
    detector.adjustRect(r);
    Rect crop_region((int)r.tl().x + 2, (int)r.tl().y +2, (int)(r.br().x - r.tl().x) +2, (int)(r.br().y - r.tl().y) +2);
    rectangle(rgb_frame, r.tl(), r.br(), Scalar(0, 255, 0), 2);
    ros::Time time = ros::Time::now();
    cv_ptr->encoding = "bgr8";
    cv_ptr->header.stamp = time;
    cv_ptr->header.frame_id = "/detection_frame";
    cv_ptr->image = rgb_frame;
    img_pub.publish(cv_ptr->toImageMsg());
  }
}

int main(int argc, char **argv) {
  ros::init(argc, argv, "hog_detector");
  ros::NodeHandle n;
  
  message_filters::Subscriber<sensor_msgs::Image> depth_sub(n, "/camera/depth/image_raw", 1);
  message_filters::Subscriber<sensor_msgs::Image> rgb_sub(n, "/camera/rgb/image_raw", 1);
  typedef sync_policies::ApproximateTime<sensor_msgs::Image, sensor_msgs::Image> MySyncPolicy;
  Synchronizer<MySyncPolicy> sync(MySyncPolicy(10), rgb_sub, depth_sub);
  sync.registerCallback(boost::bind(&RGBDcallback, _1, _2));

  image_transport::ImageTransport it(n);
  img_pub = it.advertise("/detection_frame", 1);

  ros::spin();
  return 0;
}
