//wsn reinstated 4/19/16;
// added boolean topic:
//rostopic pub gripper_open_close std_msgs/Bool 0 (for open) or 1 (for close)

//wsn, 11/15; compile low-level C-code for Dynamixel communication w/ C++ ROS node
// this node subscribes to topic "dynamixel_motor2_cmd" for position commands in the range 0-4095
// It also publishes motor angles on topic "dynamixel_motor2_ang"
// for baxter gripper, want: motor_id=1; baudnum=1; ttynum=0;
// these are the defaults;
// alternatively, run w/: rosrun baxter_gripper dynamixel_motor_node -m 2 -tty 0 -baudnum 1
// to specify motor 2; or accept tty and baudnum defaults w/:
// rosrun baxter_gripper dynamixel_motor_node -m 2

// this node's name and its topic names are mangled to match motor_id, e.g.
//  dynamixel_motor2_cmd for motor 2, and dynamixel_motor2_ang for motor2 feedback topic
//  NOTE: there are fairly frequent read errors from the motor;  
//  read errors are published as ang+4096;  inspect the angle value, and if >4096, DO NOT BELIEVE IT

// can test this node with "rosrun baxter_gripper dynamixel_sin_test", which will prompt for the motor_id,
// then command slow sinusoides on the chosen motor topic


#include<ros/ros.h> 
#include<std_msgs/Int16.h> 
#include<std_msgs/Bool.h> 
#include <linux/serial.h>
#include <termios.h>

// Default settings: EDIT THESE FOR YOUR MOTOR
#define DEFAULT_BAUDNUM		1 // code "1" --> 1Mbps
#define DEFAULT_ID		1 //this is the motor ID
#define DEFAULT_TTY_NUM			1 // typically, 0 for /dev/ttyUSB0

const short int OPEN_VALUE=1500;
const short int CLOSE_VALUE=2400;
const int CCW_LIM=4000;
const int CW_LIM=1;

extern "C" { 
  int send_dynamixel_goal(short int motor_id, short int goalval); 
  int open_dxl(int deviceIndex, int baudnum);
  //make these global, so connection info is not lost after fnc call
  char	gDeviceName[20];
  struct termios newtio;
  struct serial_struct serinfo;
  char dev_name[100] = {0, };
  void dxl_terminate(void);
  short int read_position(short int motor_id);
  void print_info(short int motor_id);
  int set_dynamixel_CCW_limit(short int motor_id, int CCW_limit);
  int set_dynamixel_CW_limit(short int motor_id, int CW_limit);
}

//globals:
  short int motor_id = DEFAULT_ID;
  short int baudnum = DEFAULT_BAUDNUM;
  int ttynum = DEFAULT_TTY_NUM;
  short int g_goal_angle=0;

void dynamixelCB(const std_msgs::Int16& goal_angle_msg) 
{ 
  short int goal_angle = goal_angle_msg.data;
  ROS_INFO("received command angle %d",goal_angle);
  g_goal_angle = goal_angle; // for use by main()
     send_dynamixel_goal(motor_id,goal_angle);
} 

void open_close_CB(const std_msgs::Bool& bool_msg) 
{ 
  short int goal_angle;
  ROS_INFO("received gripper command");
  if (bool_msg.data) {
  ROS_INFO("commanding close gripper, %d",CLOSE_VALUE);

  goal_angle = CLOSE_VALUE;
  send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
  }
  else {
  ROS_INFO("commanding open gripper, %d",OPEN_VALUE);
  goal_angle = OPEN_VALUE;
  send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
    send_dynamixel_goal(motor_id,goal_angle);
  ros::Duration(0.1).sleep();
  }  
} 


int main(int argc, char **argv) 
{ 
  std::string dash_m = "-m";
  std::string dash_tty = "-tty";
  std::string dash_baud = "-baud";
  
  if (argc<2) {
   ROS_INFO("using default motor_id %d, baud code %d, via /dev/ttyUSB%d",motor_id,baudnum,ttynum);
   ROS_INFO("may run with command args, e.g.: -m 2 -tty 1 for motor_id=2 on /dev/ttyUSB1");
  }
  else {
   std::vector <std::string> sources;
    for (int i = 1; i < argc; ++i) { // argv[0] is the path to the program, we want from argv[1] onwards
            sources.push_back(argv[i]); 
    }
    for (int i=0;i<argc-2;i++) {  // if have a -m or -tty, MUST have at least 2 args
       std::cout<<sources[i]<<std::endl;
       if (sources[i]==dash_m) {
        std::cout<<"found dash_m"<<std::endl;
        motor_id = atoi(sources[i+1].c_str()); 
 
        }
       if (sources[i]==dash_tty) {
        std::cout<<"found dash_tty"<<std::endl;
        ttynum = atoi(sources[i+1].c_str()); 
        }
       if (sources[i]==dash_baud) {
        std::cout<<"found dash_baud"<<std::endl;
        baudnum = atoi(sources[i+1].c_str()); 
        }
    }
    ROS_INFO("using motor_id %d at baud code %d via /dev/ttyUSB%d",motor_id,baudnum,ttynum);
  }
  
  char node_name[50];
  char in_topic_name[50];
  char out_topic_name[50];
  sprintf(node_name,"dynamixel_motor%d",motor_id);
  ROS_INFO("node name: %s",node_name);
  sprintf(in_topic_name,"dynamixel_motor%d_cmd",motor_id);
  ROS_INFO("input command topic: %s",in_topic_name);
  sprintf(out_topic_name,"dynamixel_motor%d_ang",motor_id);
  ROS_INFO("output topic: %s",out_topic_name);

  ros::init(argc,argv,node_name); //name this node 

  ros::NodeHandle n; // need this to establish communications with our new node 
  ros::Publisher pub_jnt = n.advertise<std_msgs::Int16>(out_topic_name, 1);
  
  double dt= 0.01; // 100Hz

  ROS_INFO("attempting to open /dev/ttyUSB%d",ttynum);
  bool open_success = open_dxl(ttynum,baudnum);

  if (!open_success) {
    ROS_WARN("could not open /dev/ttyUSB%d; check permissions?",ttynum);
    return 0;
  }
  else {
     ROS_INFO("opened /dev/ttyUSB%d",ttynum);
  }

  ROS_INFO("attempting communication with motor_id %d at baudrate code %d",motor_id,baudnum);

  ros::Subscriber subscriber = n.subscribe(in_topic_name,1,dynamixelCB); 
  ros::Subscriber open_close_subscriber = n.subscribe("gripper_open_close",1,open_close_CB);
  std_msgs::Int16 motor_ang_msg;
  short int sensed_motor_ang=0;
  int alive=0;
  while(ros::ok()) {
   alive++;
   if (alive>100) {
      alive=0;
   set_dynamixel_CCW_limit(motor_id, CCW_LIM);
   set_dynamixel_CW_limit(motor_id, CW_LIM);
      ROS_INFO("gripper driver is alive");
      print_info(motor_id);
   }
   sensed_motor_ang = read_position(motor_id);
   if (sensed_motor_ang>4096) {
      ROS_WARN("read error from Dynamixel: ang value %d at cmd %d",sensed_motor_ang-4096,g_goal_angle);
    }
    else{
    motor_ang_msg.data = sensed_motor_ang;
     pub_jnt.publish(motor_ang_msg);
   }
   ros::Duration(dt).sleep();
   ros::spinOnce();
   }
  dxl_terminate();
  ROS_INFO("goodbye");
  return 0; // should never get here, unless roscore dies 
} 
