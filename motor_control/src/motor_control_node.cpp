#include "../include/motor_control/motor_control_node.hpp"

MotorControl::MotorControl()
    : Node("motor_control"), encoder_l(0), encoder_r(0), node(this) {
  auto qos_profile = rclcpp::QoS(rclcpp::KeepLast(10));

  robot_motor_client_ = this->create_client<robot_motor>("/robot_motor");

  using namespace std::placeholders;
  motor_control_action_server_ = rclcpp_action::create_server<motor_control>(
      this->get_node_base_interface(), this->get_node_clock_interface(),
      this->get_node_logging_interface(), this->get_node_waitables_interface(),
      "/motor_control", std::bind(&MotorControl::handle_goal, this, _1, _2),
      std::bind(&MotorControl::handle_cancel, this, _1),
      std::bind(&MotorControl::motor_controling, this, _1));
}
MotorControl::~MotorControl() {}

rclcpp_action::GoalResponse
MotorControl::handle_goal(const rclcpp_action::GoalUUID &uuid,
                          std::shared_ptr<const motor_control::Goal> goal) {
  return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
}
rclcpp_action::CancelResponse MotorControl::handle_cancel(
    const std::shared_ptr<goal_handle_motor_control> goal_handle) {
  return rclcpp_action::CancelResponse::ACCEPT;
}
void MotorControl::motor_controling(
    const std::shared_ptr<goal_handle_motor_control> goal_handle) {

  std::chrono::system_clock::time_point start =
      std::chrono::system_clock::now();

  int motor_mode = goal_handle->get_goal()->mode;
  std::vector<float> motor_data = goal_handle->get_goal()->data;
  rclcpp::Rate rate(100);
  bool end_ok = false;
  while (!end_ok) {
    std::shared_ptr<robot_motor::Request> robot_req;
    switch (motor_mode) {
    case 0: {
      robot_req->motor_l = motor_data[0];
      robot_req->motor_r = motor_data[1];
      end_ok = true;
      break;
    }
    }
    auto result = robot_motor_client_->async_send_request(robot_req);
    // Wait for the result.
    if (rclcpp::spin_until_future_complete(node, result) ==
        rclcpp::FutureReturnCode::SUCCESS) {
      encoder_l = result.get()->encoder_l;
      encoder_r = result.get()->encoder_r;
    } else {
      RCLCPP_WARN(this->get_logger(),
                  "robot motor service failed\ncheck beagle_robot node");
      return;
    }
    std::shared_ptr<motor_control::Feedback> feedback;
    feedback->encoder_l = encoder_l;
    feedback->encoder_r = encoder_r;
    std::chrono::milliseconds time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start);
    feedback->time = time.count();
    goal_handle->publish_feedback(feedback);
    rate.sleep();
  }

  std::chrono::milliseconds time =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - start);
  std::shared_ptr<motor_control::Result> result;
  result->endtime = time.count();
  goal_handle->succeed(result);
}