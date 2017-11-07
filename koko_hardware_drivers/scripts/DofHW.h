#include <hardware_interface/joint_command_interface.h>
#include <hardware_interface/joint_state_interface.h>
#include <hardware_interface/robot_hw.h>
#include <sensor_msgs/JointState.h>
#include <std_msgs/Float64.h>
#include <ros/node_handle.h>
#include <vector>
#include <string>
#include <math.h>
class DofHW: public hardware_interface::RobotHW
{


public:
  DofHW(ros::NodeHandle &nh) 
  { 

    if (!nh.getParam("joint_names", joint_names)) {
      ROS_ERROR("No joint_names given (namespace: %s)", nh.getNamespace().c_str());
    }
    if (!nh.getParam("motor_names", motor_names)) {
      ROS_ERROR("No motor_names given (namespace: %s)", nh.getNamespace().c_str());
    }
    if (!nh.getParam("gear_ratio", gear_ratios)) {
      ROS_ERROR("No gear_ratio given (namespace: %s)", nh.getNamespace().c_str());
    }
    if (!nh.getParam("directions", directions)) {
      ROS_ERROR("No gear_ratio given (namespace: %s)", nh.getNamespace().c_str());
    }
    if (!nh.getParam("current_slope", current_slope)) {
      ROS_ERROR("No current_slope given (namespace: %s)", nh.getNamespace().c_str());
    }
    if (!nh.getParam("current_offset", current_offset)) {
      ROS_ERROR("No current_offset given (namespace: %s)", nh.getNamespace().c_str());
    }

    if (!nh.getParam("simple_controller/paired_constraints", paired_constraints)) {
      ROS_ERROR("No simple_controller/paired_constraints given (namespace: %s", nh.getNamespace().c_str());
    }

    if (paired_constraints.size() % 2 != 0) {
      ROS_ERROR("Paired_constraints length must be even");
    }
    


    motor_pos.resize(joint_names.size());
    motor_vel.resize(joint_names.size());
    cmd.resize(joint_names.size());
    pos.resize(joint_names.size());
    vel.resize(joint_names.size());
    eff.resize(joint_names.size());
    joint_state_initial.resize(joint_names.size());
    jnt_state_subscribers.resize(joint_names.size());
    jnt_cmd_publishers.resize(joint_names.size());
    angle_accumulated.resize(joint_names.size());
    angle_previous_mod.resize(joint_names.size());
    for (int i = 0; i < joint_names.size(); i++) {
      hardware_interface::JointStateHandle state_handle_a(joint_names[i], &pos[i], &vel[i], &eff[i]);
      //ROS_INFO("joint %s", joint_names[i].c_str());
      jnt_state_interface.registerHandle(state_handle_a);
    }

    registerInterface(&jnt_state_interface);

    for (int i = 0; i < joint_names.size(); i++) {
      hardware_interface::JointHandle effort_handle_a(jnt_state_interface.getHandle(joint_names[i]), &cmd[i]);
      jnt_effort_interface.registerHandle(effort_handle_a);
    }
    registerInterface(&jnt_effort_interface);

    position_read = 0;
    calibrated = 0;
    
    for (int i = 0; i < joint_names.size(); i++) {
      joint_state_initial[i] = 0.0;
    }
     
    calibration_num = 50;
 

    jnt_state_tracker_subscriber = nh.subscribe("joint_state_tracker", 1000, &DofHW::CalibrateJointState, this);

    for (int i = 0; i < motor_names.size(); i++) {
      jnt_state_subscribers[i] = nh.subscribe(motor_names[i] + "_State", 1000, &DofHW::UpdateJointState, this);
    }
    
    for (int i = 0; i < motor_names.size(); i++) {
      jnt_cmd_publishers[i] = nh.advertise<std_msgs::Float64>(motor_names[i] + "_Cmd", 1000);
    }
  }
  
  void UpdateJointState(const sensor_msgs::JointState msg) {


    int index = -1;
    
    for (int i = 0; i < motor_names.size(); i++) {
      // ROS_ERROR("joint name comparing: %s, to %s, with result %d", joint_names[i].c_str(), msg.name[0].c_str(), msg.name[0].compare(joint_names[i]));
      if (msg.name[0].compare(motor_names[i]) == 0){ 
        index = i;
        // ROS_ERROR("Index Value after setting: %d", index);
      }
    }

    if (index == -1){
      ROS_ERROR("Some Joint DofHW error, msg name %s, with %ld joints", msg.name[0].c_str(), joint_names.size());
    }

    if (angle_previous_mod[index] == 0) {
      angle_previous_mod[index] = msg.position[0];
    } else if (is_calibrated == 1){

      double mod_angle = msg.position[0];

      double delta_angle = mod_angle - angle_previous_mod[index];
      delta_angle = std::fmod(delta_angle + M_PI, 2 * M_PI);
      if (delta_angle< 0) delta_angle +=  2 * M_PI;
      delta_angle = delta_angle -  M_PI;

      angle_previous_mod[index] = mod_angle;

      angle_accumulated[index] += delta_angle; 
      ROS_ERROR("%d", index);
      motor_pos[index] = angle_accumulated[index];
      motor_vel[index] = msg.velocity[0];

      double pre_pos = angle_accumulated[index];
      double pre_vel = msg.velocity[0];


      if(std::find(paired_constraints.begin(), paired_constraints.end(), index) != paired_constraints.end()) {
        int a = std::find(paired_constraints.begin(), paired_constraints.end(), index) - paired_constraints.begin();
        //trying to set index a
        if (a % 2 == 0) {
          int b = a + 1;
          //Might have to change signs
          pre_pos = -.5 * motor_pos[paired_constraints[a]] + .5 * motor_pos[paired_constraints[b]];
          pre_vel = -.5 * motor_vel[paired_constraints[a]] + .5 * motor_vel[paired_constraints[b]];

        } else {
          int b = a - 1;
          pre_pos = .5 * motor_pos[paired_constraints[b]] + .5 * motor_pos[paired_constraints[a]];
          pre_vel = .5 * motor_vel[paired_constraints[b]] + .5 * motor_vel[paired_constraints[a]];
        }
      } 
      if (index ==2 ) {
        ROS_ERROR("directions index %f",  directions[2]);
      }

      pos[index] = directions[index] * (pre_pos / gear_ratios[index]) + joint_state_initial[index];
      vel[index] = directions[index] * pre_vel / gear_ratios[index];
      eff[index] = directions[index] * msg.effort[0] * gear_ratios[index];
      position_read = 1;
    }

  }

  void CalibrateJointState(const sensor_msgs::JointState msg) {
    ROS_ERROR("calibrated index %d", calibrated);
    if (calibrated == calibration_num)
    {
      for (int i = 0; i < joint_state_initial.size(); i++) {
        joint_state_initial[i] = msg.position[i];
        pos[i] = joint_state_initial[i];
        ROS_ERROR("calibrated initial joint state %f", joint_state_initial[i]);
      }
      calibrated++;
      is_calibrated = 1; 
    }
    else if(calibrated != calibration_num + 1)
    {
      for (int i = 0; i < joint_state_initial.size(); i++) {
        joint_state_initial[i] = msg.position[i];
        pos[i] = joint_state_initial[i];
        ROS_ERROR("calibrated initial joint state %f", joint_state_initial[i]);
      }
      calibrated++;
    }
  }

  double convertMotorTorqueToCurrent(double motor_torque, int index) {
    return current_slope[index] * motor_torque + current_offset[index];
  }
  
  ros::Time get_time() {
    return ros::Time::now();
  } 

  ros::Duration get_period() {
    ros::Time current_time = ros::Time::now();
    ros::Duration period = current_time - last_time; 
    last_time = current_time;
    return period; 
  }

  void read() {
  }

  void write() {
    PublishJointCommand();
  }
  
  void PublishJointCommand() {

    if (calibrated == calibration_num + 1) {
      std::vector<double> pre(joint_names.size());
      for (int k = 0; k < joint_names.size(); k++) {
        pre[k] = cmd[k];
        //ROS_INFO("cmd %d = %f", k, cmd[k]);
      }

      for (int j = 0; j < paired_constraints.size(); j = j + 2) {
        double index_a = paired_constraints[j];
        double index_b = paired_constraints[j + 1];
        pre[index_a] = -.5 * cmd[index_a] + .5 * cmd[index_b];
        pre[index_b] = .5 * cmd[index_a] + .5 * cmd[index_b];
      }    

      for (int i = 0; i < joint_names.size(); i++) {

        double motor_torque =  pre[i] / gear_ratios[i];
        double motor_current = convertMotorTorqueToCurrent(motor_torque, i);
        std_msgs::Float64 commandMsg; 
        commandMsg.data =  motor_current;
        jnt_cmd_publishers[i].publish(commandMsg);
        //ROS_ERROR("current command for %s is %f", joint_names[i].c_str(), commandMsg.data);
      }
    }
  }
  
  const int getPositionRead() {
    return position_read;
  }

private:
  hardware_interface::JointStateInterface jnt_state_interface;
  hardware_interface::EffortJointInterface jnt_effort_interface;
  std::vector<ros::Subscriber> jnt_state_subscribers;
  std::vector<ros::Publisher> jnt_cmd_publishers;
  ros::Subscriber jnt_state_tracker_subscriber;
  ros::Time last_time;
  std::vector<std::string> joint_names; 
  std::vector<std::string> motor_names; 
  std::vector<double> gear_ratios;
  std::vector<double> cmd;
  std::vector<double> pos;
  std::vector<double> vel;
  std::vector<double> eff;  
  std::vector<double> current_slope;
  std::vector<double> current_offset;
  std::vector<int> paired_constraints;
  std::vector<double> motor_pos;
  std::vector<double> motor_vel;  
  int position_read;
  int calibrated;
  std::vector<double> joint_state_initial;
  std::vector<double> directions;
  double i_to_T_slope;
  double i_to_T_intercept;
  int calibration_num;
  std::vector<double> angle_accumulated; 
  std::vector<double> angle_previous_mod;
  int is_calibrated;
};