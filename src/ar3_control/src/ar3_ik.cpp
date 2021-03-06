#include <iostream>
#include <string>
#include <vector>
 
#include "ros/ros.h"
#include "sensor_msgs/JointState.h"
#include "turtlesim/Pose.h"
#include "tf2_ros/static_transform_broadcaster.h"
#include "tf2_ros/transform_broadcaster.h"
#include "geometry_msgs/TransformStamped.h"
#include "tf2/LinearMath/Quaternion.h"
#include "kdl_parser/kdl_parser.hpp"
#include "kdl/chainiksolverpos_nr_jl.hpp"
#include "trac_ik/trac_ik.hpp"
#include "urdf/model.h"
#define pi 3.141592653
 
int main(int argc, char* argv[])
{
    setlocale(LC_ALL,"");
    ros::init(argc,argv,"dynamic_tf_pub");
    ros::NodeHandle nh;
    ros::Publisher joint_pub = nh.advertise<sensor_msgs::JointState>("joint_states", 1);
    //定义tf坐标广播器
    tf2_ros::StaticTransformBroadcaster broadcaster;
    geometry_msgs::TransformStamped ts;
    KDL::Vector v1(1,1,1);
    KDL::Tree my_tree;
    sensor_msgs::JointState joint_state;
    
    std::string robot_desc_string;
    nh.param("robot_description", robot_desc_string, std::string());
    
    if(!kdl_parser::treeFromString(robot_desc_string, my_tree))
    //if(!kdl_parser::treeFromFile("/home/zhitong/catkin_ws_serial/src/quadruped_9g-master/urdf/mbot_base.urdf", my_tree))
    {
        ROS_ERROR("Failed to construct kdl tree");
    }
    else
    {
        ROS_INFO("成功生成kdl树!");
    }
    std::vector<std::string> joint_name = {"joint_1", "joint_2", "joint_3", "joint_4","joint_5","joint_6"};
    std::vector<double> joint_pos = {0,0,0,0,0,0};
    
    std::string urdf_param = "/robot_description";
    double timeout = 0.005;
    double eps = 1e-5;
    std::string chain_start = "base_link"; 
    std::string chain_end = "link_6"; 
    TRAC_IK::TRAC_IK tracik_solver(chain_start, chain_end, urdf_param, timeout, eps);
    KDL::Chain chain;
    KDL::JntArray ll, ul; //关节下限, 关节上限
    bool valid = tracik_solver.getKDLChain(chain);
    if(!valid)
    {
        ROS_ERROR("There was no valid KDL chain found");
    }
    valid = tracik_solver.getKDLLimits(ll, ul);
    if(!valid)
    {
        ROS_ERROR("There was no valid KDL joint limits found");
    }
    KDL::ChainFkSolverPos_recursive fk_solver(chain);
    ROS_INFO("关节数量: %d", chain.getNrOfJoints());
    KDL::JntArray nominal(6);
    ROS_INFO("the nominal size is:%d",nominal.data.size());
    for(size_t j = 0; j < 6; j ++)
    {
        nominal(j)=0.0;
        //nominal(j) = (ll(j) + ul(j))/2.0;
    }
    KDL::JntArray q(6); // 目标关节位置
    q(0)=0;
    q(1)=pi/3;
    q(2)=pi/3;
    q(3)=0;
    q(4)=0;
    q(5)=0;
    q(6)=0;
    //定义末端4x4齐次变换矩阵
    KDL::Frame end_effector_pose;
    //定义逆运动学解算结果存储数组
    KDL::JntArray result;
    ros::Rate r(1);
 
    bool flag=true;
    auto print_frame_lambda = [](KDL::Frame f)
    {
        double x, y, z, roll, pitch, yaw;
        x = f.p.x();
        y = f.p.y();
        z = f.p.z();
        f.M.GetRPY(roll, pitch, yaw);
        std::cout << "x:" << x << " y:" << y << " z:" << z << " roll:" << roll << " pitch:" << pitch << " yaw:" << yaw << std::endl;
    };
 
    //正运动学
    fk_solver.JntToCart(q,end_effector_pose);
    //逆运动学
    int rc = tracik_solver.CartToJnt(nominal, end_effector_pose, result);
    
    ROS_INFO("1:%f,2:%f,3:%f,4:%f,5:%f,6:%f",result(0),result(1),result(2),result(3),result(4),result(5));
    print_frame_lambda(end_effector_pose);
    
    
    while(ros::ok())
    {
    if(flag)
        {
            fk_solver.JntToCart(q, end_effector_pose);
            int rc = tracik_solver.CartToJnt(nominal, end_effector_pose, result);
            
            print_frame_lambda(end_effector_pose);
        }
        else
        {
            fk_solver.JntToCart(nominal, end_effector_pose); 
            int rc = tracik_solver.CartToJnt(q, end_effector_pose, result);
            print_frame_lambda(end_effector_pose);
        }
        flag = !flag;
        // 更新关节状态
        ROS_INFO("更新关节状态");
        joint_state.header.stamp = ros::Time::now();
	//ROS_INFO("%d",joint_state.header.stamp);
        joint_state.name.resize(6);
        joint_state.position.resize(6);
 
        for(size_t i = 0; i < 6; i ++)
        {
            joint_state.name[i] = joint_name[i];
            //joint_state.position[i] = result(i);
        }
    joint_state.position[0] = result(0);
    joint_state.position[1] = result(1);
    joint_state.position[2] = result(2);
    joint_state.position[3] = result(3);
    joint_state.position[4] = result(4);
    joint_state.position[5] = result(5);
    joint_pub.publish(joint_state);
    
    r.sleep();
    }
    return 0;
}