#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/path.hpp"
#include "std_srvs/srv/trigger.hpp"
#include "tf2/exceptions.hpp"
#include "tf2_ros/buffer.hpp"
#include "tf2_ros/transform_listener.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include <chrono>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

using geometry_msgs::msg::PoseStamped;
using geometry_msgs::msg::TransformStamped;
using geometry_msgs::msg::Twist;
using nav_msgs::msg::Path;
using std::string;
using namespace std::chrono_literals;

class PathFollower : public rclcpp::Node
{
public:
  PathFollower() : Node("path_follower")
  {
    state = WAITING_FOR_PATH;
    auto path_callback = [this](Path::UniquePtr path) -> void {
      if (path->poses.empty())
      {
        clear_path();
        return;
      }
      if (path->poses.size() < 2)
      {
        RCLCPP_ERROR(this->get_logger(), "Path is too short, it has only %zu points and should have at least 2",
                     path->poses.size());
        return;
      }
      pose_id = 1;
      RCLCPP_INFO(this->get_logger(), "Path registered");
      PoseStamped goal = path->poses.back();
      RCLCPP_INFO(this->get_logger(), "Goal: (%.2f, %.2f, %.2f)", goal.pose.position.x, goal.pose.position.y,
                  goal.pose.position.z);
      RCLCPP_INFO(this->get_logger(), "Changing state to FOLLOWING_PATH");
      state = FOLLOWING_PATH;
      this->path = std::move(path);
    };
    auto timer_callback = [this]() -> void { follow_path(); };
    subscription = this->create_subscription<Path>("/path", 10, path_callback);
    publisher = this->create_publisher<Twist>("/cmd_vel", 10);
    stop_service = this->create_service<std_srvs::srv::Trigger>(
        "/stop_path_follower",
        [this](const std_srvs::srv::Trigger::Request::SharedPtr,
               std_srvs::srv::Trigger::Response::SharedPtr response) {
          clear_path();
          response->success = true;
          response->message = "Path follower stopped";
        });
    tf_buffer = std::make_unique<tf2_ros::Buffer>(this->get_clock());
    tf_listener = std::make_unique<tf2_ros::TransformListener>(*tf_buffer);
    timer = this->create_timer(100ms, timer_callback);
  }

private:
  using DiscreteState = std::vector<int>;
  using Action = int;
  using QTable = std::map<DiscreteState, std::map<Action, double>>;

  rclcpp::Subscription<Path>::SharedPtr subscription;
  rclcpp::Publisher<Twist>::SharedPtr publisher;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr stop_service;
  std::unique_ptr<tf2_ros::Buffer> tf_buffer;
  std::unique_ptr<tf2_ros::TransformListener> tf_listener;
  rclcpp::TimerBase::SharedPtr timer;
  string fixed_frame = "map";
  string robot_frame = "base_link";
  double head_radius = 0.4;
  QTable q_table;
  void follow_path();
  std::optional<PoseStamped> find_head_point();
  DiscreteState discretize_state(const std::vector<double>& observation);
  Action choose_action(const DiscreteState& state);
  void execute_action(Action action);
  void update_q_table(const DiscreteState& state, Action action, double reward,
                      const DiscreteState& next_state, bool terminal);
  void save_q_table(const string& filename) const;
  void load_q_table(const string& filename);
  void learning_loop();
  void stop_moving();
  void clear_path();
  PoseStamped global_to_local(PoseStamped point);
  double distance(PoseStamped point);
  int state;
  int pose_id;
  Path::UniquePtr path;
  enum State
  {
    WAITING_FOR_PATH,
    FOLLOWING_PATH,
  };
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PathFollower>());
  rclcpp::shutdown();
}

void PathFollower::follow_path()
{
}

std::optional<PoseStamped> PathFollower::find_head_point()
{
  if (!path || path->poses.empty())
  {
    RCLCPP_WARN(get_logger(), "Cannot find head point: no path");
    return std::nullopt;
  }
  try
  {
    PoseStamped end = global_to_local(path->poses.back());
    if (distance(end) <= head_radius)
    {
      return end;
    }

    for (size_t i = path->poses.size() - 1; i > 0; --i)
    {
      PoseStamped start = global_to_local(path->poses[i - 1]);
      const double x = start.pose.position.x;
      const double y = start.pose.position.y;
      const double dx = end.pose.position.x - x;
      const double dy = end.pose.position.y - y;

      // Przecięcie okręgu z odcinkiem (x + t*dx, y + t*dy), 0 <= t <= 1.
      const double a = dx * dx + dy * dy;
      const double b = 2.0 * (x * dx + y * dy);
      const double c = x * x + y * y - head_radius * head_radius;
      const double discriminant = b * b - 4.0 * a * c;
      if (a > 0.0 && discriminant >= 0.0)
      {
        const double root = std::sqrt(discriminant);
        // Większe t leży bliżej końca trasy.
        const double candidates[] = {(-b + root) / (2.0 * a), (-b - root) / (2.0 * a)};
        for (const double t : candidates)
        {
          if (t >= 0.0 && t <= 1.0)
          {
            PoseStamped point = end;
            point.pose.position.x = x + t * dx;
            point.pose.position.y = y + t * dy;
            point.pose.position.z = start.pose.position.z +
              t * (end.pose.position.z - start.pose.position.z);
            return point;
          }
        }
      }
      end = std::move(start);
    }
  }
  catch (const tf2::TransformException& ex)
  {
    RCLCPP_WARN(get_logger(), "Cannot find head point: %s", ex.what());
    return std::nullopt;
  }
  RCLCPP_WARN(get_logger(), "Cannot find head point: path does not intersect the lookahead circle");
  return std::nullopt;
}

PathFollower::DiscreteState PathFollower::discretize_state(const std::vector<double>&)
{
  throw std::logic_error("discretize_state: not implemented");
}

PathFollower::Action PathFollower::choose_action(const DiscreteState&)
{
  throw std::logic_error("choose_action: not implemented");
}

void PathFollower::execute_action(Action)
{
  throw std::logic_error("execute_action: not implemented");
}

void PathFollower::update_q_table(const DiscreteState&, Action, double, const DiscreteState&, bool)
{
  throw std::logic_error("update_q_table: not implemented");
}

void PathFollower::save_q_table(const string&) const
{
  throw std::logic_error("save_q_table: not implemented");
}

void PathFollower::load_q_table(const string&)
{
  throw std::logic_error("load_q_table: not implemented");
}

void PathFollower::learning_loop()
{
  throw std::logic_error("learning_loop: not implemented");
}

PoseStamped PathFollower::global_to_local(PoseStamped point)
{
  point.header.stamp.sec = 0;
  point.header.stamp.nanosec = 0;
  return tf_buffer->transform(point, robot_frame);
}

void PathFollower::stop_moving()
{
  Twist twist;
  publisher->publish(twist);
}

void PathFollower::clear_path()
{
  state = WAITING_FOR_PATH;
  pose_id = 1;
  path.reset();
  stop_moving();
  RCLCPP_INFO(this->get_logger(), "Path cleared, waiting for a new path");
}

double PathFollower::distance(PoseStamped point)
{
  return hypot(point.pose.position.x, point.pose.position.y);
}