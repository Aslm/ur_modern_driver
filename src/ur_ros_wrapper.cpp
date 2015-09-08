/*
 * ur_driver.cpp
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <thomas.timm.dk@gmail.com> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Thomas Timm Andersen
 * ----------------------------------------------------------------------------
 */

#include "ros/ros.h"
#include <ros/console.h>
#include "ur_modern_driver/ur_driver.h"
#include <string.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "sensor_msgs/JointState.h"
#include "geometry_msgs/WrenchStamped.h"
#include "ur_msgs/RobotStateRTMsg.h"

UrDriver* g_robot;
std::mutex g_msg_lock;
std::condition_variable g_msg_cond;
std::vector<std::string> g_joint_names;

void publishRTMsg() {
	ros::NodeHandle n_rt;
	ros::Publisher joint_pub = n_rt.advertise<sensor_msgs::JointState>("/joint_states", 1);
	ros::Publisher wrench_pub = n_rt.advertise<geometry_msgs::WrenchStamped>("/wrench", 1);
	while (ros::ok()) {
		sensor_msgs::JointState joint_msg;
		joint_msg.name = g_joint_names;
		geometry_msgs::WrenchStamped wrench_msg;
		std::mutex msg_lock; // The values are locked for reading in the class, so just use a dummy mutex
		std::unique_lock<std::mutex> locker(msg_lock);
		while (!g_robot->rt_interface_->robot_state_->getNewDataAvailable()) {
			g_msg_cond.wait(locker);
		}
		joint_msg.header.stamp = ros::Time::now();
		joint_msg.position = g_robot->rt_interface_->robot_state_->getQActual();
		joint_msg.velocity = g_robot->rt_interface_->robot_state_->getQdActual();
		joint_msg.effort = g_robot->rt_interface_->robot_state_->getIActual();
		joint_pub.publish(joint_msg);
		std::vector<double> tcp_force = g_robot->rt_interface_->robot_state_->getTcpForce();
		wrench_msg.header.stamp = joint_msg.header.stamp;
		wrench_msg.wrench.force.x = tcp_force[0];
		wrench_msg.wrench.force.y = tcp_force[1];
		wrench_msg.wrench.force.z = tcp_force[2];
		wrench_msg.wrench.torque.x = tcp_force[3];
		wrench_msg.wrench.torque.y = tcp_force[4];
		wrench_msg.wrench.torque.z = tcp_force[5];
		wrench_pub.publish(wrench_msg);

		g_robot->rt_interface_->robot_state_->finishedReading();

	}
}

int main(int argc, char **argv) {
	bool use_sim_time = false;
	std::string joint_prefix ="";
	std::string host;

	ros::init(argc, argv, "ur_driver");
	ros::NodeHandle n;
	if (ros::param::get("use_sim_time", use_sim_time)) {
		ROS_WARN("use_sim_time is set!!!");
	}
	if (ros::param::get("~prefix", joint_prefix)) {
		ROS_INFO("Setting prefix to %s", joint_prefix.c_str());
	}
	g_joint_names.push_back(joint_prefix + "shoulder_pan_joint");
	g_joint_names.push_back(joint_prefix + "should_lift_joint");
	g_joint_names.push_back(joint_prefix + "elbow_joint");
	g_joint_names.push_back(joint_prefix + "wrist_1_joint");
	g_joint_names.push_back(joint_prefix + "wrist_2_joint");
	g_joint_names.push_back(joint_prefix + "wrist_3_joint");

	if (argc > 1) {
		host = argv[1];
	} else if (!(ros::param::get("~robot_ip", host))) {
		ROS_FATAL("Could not get robot ip. Please supply it as command line parameter or on the parameter server as robot_ip");
		exit(1);
	}
	g_robot = new UrDriver(g_msg_cond, host);
	g_robot->start();
	publishRTMsg();
	ros::spin();
	g_robot->halt();
	exit(0);
}
