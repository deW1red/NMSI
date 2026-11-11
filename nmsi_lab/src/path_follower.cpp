#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "nav_msgs/msg/occupancy_grid.hpp"
#include "nav_msgs/msg/path.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_srvs/srv/empty.hpp"
#include "std_srvs/srv/trigger.hpp"
#include "tf2/exceptions.hpp"
#include "tf2/utils.hpp"
#include "tf2_ros/buffer.hpp"
#include "tf2_ros/transform_listener.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <random>
#include <sstream>
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
    epsilon = declare_parameter("epsilon", 0.2);
    reset_settle_time = declare_parameter("reset_settle_time", 1.0);
    path_settle_time = declare_parameter("path_settle_time", 1.0);
    q_file = declare_parameter("q_file", string("q_table_path_only.txt"));
    random.seed(declare_parameter("random_seed", 42));
    if (epsilon < 0.0 || epsilon > 1.0)
      throw std::invalid_argument("epsilon must be between 0 and 1");
    if (!std::isfinite(reset_settle_time) || reset_settle_time < 0.0 ||
        !std::isfinite(path_settle_time) || path_settle_time < 0.0)
      throw std::invalid_argument("Settling times must be finite and nonnegative");
    load_q_table(q_file);
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
      if (training && (training_phase != WAITING_FOR_PLAN ||
          std::hypot(path->poses.back().pose.position.x - training_goal.pose.position.x,
                     path->poses.back().pose.position.y - training_goal.pose.position.y) > 0.001))
        return;
      stop_moving();
      previous_state.reset();
      episode_steps = 0;
      pose_id = 1;
      RCLCPP_INFO(this->get_logger(), "Path registered");
      PoseStamped goal = path->poses.back();
      RCLCPP_INFO(this->get_logger(), "Goal: (%.2f, %.2f, %.2f)", goal.pose.position.x, goal.pose.position.y,
                  goal.pose.position.z);
      RCLCPP_INFO(this->get_logger(), "Changing state to FOLLOWING_PATH");
      state = FOLLOWING_PATH;
      this->path = std::move(path);
      path_received_at = now();
      if (training)
      {
        failed_plans = 0;
        training_phase = EXECUTING;
      }
    };
    auto timer_callback = [this]() -> void { follow_path(); };
    subscription = this->create_subscription<Path>("/path", 10, path_callback);
    publisher = this->create_publisher<Twist>("/cmd_vel", 10);
    map_subscription = create_subscription<nav_msgs::msg::OccupancyGrid>(
        "/map_dilated", rclcpp::QoS(1).reliable().transient_local(),
        [this](nav_msgs::msg::OccupancyGrid::ConstSharedPtr msg) { map = msg; });
    amcl_subscription = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
        "/amcl_pose", 10,
        [this](geometry_msgs::msg::PoseWithCovarianceStamped::ConstSharedPtr msg) {
          if (training && training_phase == WAITING_FOR_LOCALIZATION &&
              rclcpp::Time(msg->header.stamp, get_clock()->get_clock_type()) > localization_after)
          {
            reset_settling_started = now();
            training_phase = SETTLING_AFTER_RESET;
          }
        });
    collision_subscription = create_subscription<std_msgs::msg::Bool>(
        "/collision", rclcpp::QoS(1).reliable().transient_local(),
        [this](std_msgs::msg::Bool::ConstSharedPtr msg) { collided = msg->data; });
    stop_service = this->create_service<std_srvs::srv::Trigger>(
        "/stop_path_follower",
        [this](const std_srvs::srv::Trigger::Request::SharedPtr,
               std_srvs::srv::Trigger::Response::SharedPtr response) {
          clear_path();
          response->success = true;
          response->message = "Path follower stopped";
        });
    reset_client = create_client<std_srvs::srv::Trigger>("/random_reset");
    amcl_update_client = create_client<std_srvs::srv::Empty>("/request_nomotion_update");
    goal_publisher = create_publisher<PoseStamped>("/goal_pose", 10);
    learn_service = create_service<std_srvs::srv::Trigger>(
        "/start_learning",
        [this](const std_srvs::srv::Trigger::Request::SharedPtr,
               std_srvs::srv::Trigger::Response::SharedPtr response) {
          if (training_phase == RESETTING)
          {
            response->message = "A reset is still in progress";
            return;
          }
          if (training)
          {
            response->message = "Already learning";
            return;
          }
          clear_path();
          training = true;
          training_phase = NEED_RESET;
          RCLCPP_INFO(get_logger(), "Learning started with epsilon = %.3f", epsilon);
          response->success = true;
          response->message = "Learning started";
        });
    follow_service = create_service<std_srvs::srv::Trigger>(
        "/start_following",
        [this](const std_srvs::srv::Trigger::Request::SharedPtr,
               std_srvs::srv::Trigger::Response::SharedPtr response) {
          if (training_phase == RESETTING)
          {
            response->message = "A reset is still in progress";
            return;
          }
          if (training)
          {
            try { save_q_table(q_file); }
            catch (const std::runtime_error& ex)
            {
              response->message = ex.what();
              RCLCPP_ERROR(get_logger(), "%s", ex.what());
              return;
            }
          }
          clear_path();
          training = false;
          training_phase = IDLE;
          response->success = true;
          response->message = "Following mode: waiting for a path";
        });
    tf_buffer = std::make_unique<tf2_ros::Buffer>(this->get_clock());
    tf_listener = std::make_unique<tf2_ros::TransformListener>(*tf_buffer);
    timer = this->create_timer(100ms, timer_callback);
  }

private:
  // Przykładowa reprezentacja stanu; można ją zmienić razem z typem klucza QTable.
  using DiscreteState = std::vector<int>;
  using Action = int;
  // TODO: dobierz liczbę akcji; swoje stałe (alfa, gamma, nagrody) dodaj jako pola lub parametry.
  static constexpr int action_count = 5;
  using QTable = std::map<DiscreteState, std::array<double, action_count>>;

  rclcpp::Subscription<Path>::SharedPtr subscription;
  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr map_subscription;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr amcl_subscription;
  nav_msgs::msg::OccupancyGrid::ConstSharedPtr map;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr collision_subscription;
  std::optional<bool> collided;
  rclcpp::Publisher<Twist>::SharedPtr publisher;
  rclcpp::Publisher<PoseStamped>::SharedPtr goal_publisher;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr stop_service;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr learn_service, follow_service;
  rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr reset_client;
  rclcpp::Client<std_srvs::srv::Empty>::SharedPtr amcl_update_client;
  std::unique_ptr<tf2_ros::Buffer> tf_buffer;
  std::unique_ptr<tf2_ros::TransformListener> tf_listener;
  rclcpp::TimerBase::SharedPtr timer;
  string fixed_frame = "map";
  string robot_frame = "base_link";
  double head_radius = 0.4;
  QTable q_table;
  bool training = false;
  enum TrainingPhase { IDLE, NEED_RESET, RESETTING, WAITING_FOR_LOCALIZATION, SETTLING_AFTER_RESET,
                       NEED_GOAL, WAITING_FOR_PLAN, EXECUTING };
  TrainingPhase training_phase = IDLE;
  rclcpp::Time localization_after{0, 0, RCL_ROS_TIME};
  rclcpp::Time reset_settling_started{0, 0, RCL_ROS_TIME};
  rclcpp::Time path_received_at{0, 0, RCL_ROS_TIME};
  std::chrono::steady_clock::time_point phase_started;
  PoseStamped training_goal;
  double epsilon;
  double reset_settle_time;
  double path_settle_time;
  string q_file;
  std::mt19937 random;
  std::optional<DiscreteState> previous_state;
  Action previous_action = 0;
  int episode_steps = 0;
  int failed_plans = 0;
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
  void prepare_training_episode();
  void finish_episode(const char* reason);
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
  if (training && state == WAITING_FOR_PATH)
  {
    prepare_training_episode();
    return;
  }
  if (state == WAITING_FOR_PATH || !collided.has_value())
    return;
  if ((now() - path_received_at).seconds() < path_settle_time)
    return;
  learning_loop();
}

void PathFollower::prepare_training_episode()
{
  const auto elapsed = std::chrono::steady_clock::now() - phase_started;
  if (training_phase == NEED_RESET)
  {
    if (!reset_client->service_is_ready())
    {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "Waiting for /random_reset");
      return;
    }
    if (!amcl_update_client->service_is_ready())
    {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "Waiting for /request_nomotion_update");
      return;
    }
    training_phase = RESETTING;
    phase_started = std::chrono::steady_clock::now();
    reset_client->async_send_request(
      std::make_shared<std_srvs::srv::Trigger::Request>(),
      [this](rclcpp::Client<std_srvs::srv::Trigger>::SharedFuture result) {
        const auto response = result.get();
        if (!response->success)
        {
          RCLCPP_ERROR(get_logger(), "Random reset failed: %s", response->message.c_str());
          training = false;
          training_phase = IDLE;
          return;
        }
        failed_plans = 0;
        localization_after = now();
        training_phase = WAITING_FOR_LOCALIZATION;
        phase_started = std::chrono::steady_clock::now();
        amcl_update_client->async_send_request(std::make_shared<std_srvs::srv::Empty::Request>());
        RCLCPP_INFO(get_logger(), "Waiting for AMCL after reset");
      });
    return;
  }
  if (training_phase == RESETTING && elapsed > 20s)
  {
    RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 5000,
                          "Waiting for random reset to complete; mode changes are blocked");
    return;
  }
  if (training_phase == WAITING_FOR_LOCALIZATION)
  {
    if (elapsed > 15s)
    {
      RCLCPP_ERROR(get_logger(), "AMCL did not publish a fresh pose after reset; learning stopped");
      training = false;
      training_phase = IDLE;
    }
    return;
  }
  if (training_phase == SETTLING_AFTER_RESET)
  {
    if ((now() - reset_settling_started).seconds() < reset_settle_time)
      return;
    training_phase = NEED_GOAL;
  }
  if (training_phase == WAITING_FOR_PLAN && elapsed > 10s)
  {
    ++failed_plans;
    if (failed_plans >= 5)
    {
      RCLCPP_WARN(get_logger(), "No path after %d attempts; resetting robot position", failed_plans);
      training_phase = NEED_RESET;
      return;
    }
    RCLCPP_WARN(get_logger(), "No path to random goal (%d/5); choosing another", failed_plans);
    training_phase = NEED_GOAL;
  }
  if (training_phase != NEED_GOAL || !collided.has_value() || *collided)
    return;
  if (!map || map->data.empty())
  {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "Waiting for /map_dilated");
    return;
  }
  geometry_msgs::msg::TransformStamped robot;
  try { robot = tf_buffer->lookupTransform(fixed_frame, robot_frame, tf2::TimePointZero); }
  catch (const tf2::TransformException& ex)
  {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "Waiting for robot TF: %s", ex.what());
    return;
  }
  const auto& info = map->info;
  if (map->header.frame_id != fixed_frame || info.width == 0 || info.height == 0 ||
      info.resolution <= 0.0 ||
      map->data.size() != static_cast<size_t>(info.width) * info.height)
  {
    RCLCPP_ERROR(get_logger(), "Invalid map for random goal; learning stopped");
    training = false;
    training_phase = IDLE;
    return;
  }
  const double yaw = tf2::getYaw(info.origin.orientation);
  std::uniform_int_distribution<size_t> cell(0, map->data.size() - 1);
  for (int attempt = 0; attempt < 2000; ++attempt)
  {
    const size_t index = cell(random);
    if (map->data[index] < 0 || map->data[index] >= 65) continue;
    const double x = (index % info.width + 0.5) * info.resolution;
    const double y = (index / info.width + 0.5) * info.resolution;
    training_goal.header.frame_id = fixed_frame;
    training_goal.header.stamp = now();
    training_goal.pose.position.x = info.origin.position.x + std::cos(yaw) * x - std::sin(yaw) * y;
    training_goal.pose.position.y = info.origin.position.y + std::sin(yaw) * x + std::cos(yaw) * y;
    training_goal.pose.orientation.w = 1.0;
    if (std::hypot(training_goal.pose.position.x - robot.transform.translation.x,
                   training_goal.pose.position.y - robot.transform.translation.y) < 0.8) continue;
    goal_publisher->publish(training_goal);
    training_phase = WAITING_FOR_PLAN;
    phase_started = std::chrono::steady_clock::now();
    RCLCPP_INFO(get_logger(), "Random goal: %.2f, %.2f",
                training_goal.pose.position.x, training_goal.pose.position.y);
    return;
  }
  RCLCPP_ERROR(get_logger(), "Cannot find a random free goal; learning stopped");
  training = false;
  training_phase = IDLE;
}

std::optional<PoseStamped> PathFollower::find_head_point()
{
  if (!path || path->poses.empty())
  {
    RCLCPP_WARN(get_logger(), "Cannot find head point: no path");
    return std::nullopt;
  }
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
  RCLCPP_WARN(get_logger(), "Cannot find head point: path does not intersect the lookahead circle");
  return std::nullopt;
}

// ===========================================================================
// ZADANIE: poniższe funkcje uzupełnij samodzielnie (patrz README.md).
// ===========================================================================

// TODO 1: Zamień ciągłą obserwację na dyskretny stan (indeksy przedziałów).
// Obserwację ustalasz sam (np. kąt do punktu z find_head_point(), odległość do celu).
PathFollower::DiscreteState PathFollower::discretize_state(const std::vector<double>& /*observation*/)
{
  return {};
}

// TODO 2: Wybierz akcję strategią epsilon-zachłanną (epsilon to pole klasy).
// Poza uczeniem (training == false) zawsze wybieraj najlepszą akcję.
PathFollower::Action PathFollower::choose_action(const DiscreteState& /*s*/)
{
  return 0;
}

// TODO 3: Zamień numer akcji na prędkości i opublikuj je na /cmd_vel (publisher).
void PathFollower::execute_action(Action /*action*/)
{
}

// TODO 4: Zaktualizuj Q-tablicę. Pamiętaj o stanie terminalnym.
void PathFollower::update_q_table(const DiscreteState& /*s*/, Action /*action*/, double /*reward*/,
                                  const DiscreteState& /*next*/, bool /*terminal*/)
{
}

// TODO 5: Zapisz q_table do pliku tekstowego.
void PathFollower::save_q_table(const string& /*filename*/) const
{
}

// TODO 6: Wczytaj q_table z pliku zapisanego przez save_q_table.
// Brak pliku nie jest błędem – zaczynamy wtedy z pustą tablicą.
void PathFollower::load_q_table(const string& /*filename*/)
{
}

// TODO 7: Jeden krok uczenia. Timer wywołuje tę funkcję regularnie; nie twórz tu pętli for/while uczenia.
// Do dyspozycji: collided, find_head_point(), global_to_local(), distance(), path,
// previous_state, previous_action, episode_steps, training, finish_episode().
// Zakończenie epizodu: finish_episode("powód").
void PathFollower::learning_loop()
{
}

void PathFollower::finish_episode(const char* reason)
{
  RCLCPP_INFO(get_logger(), "Episode ended: %s after %d steps, %zu states",
              reason, episode_steps, q_table.size());
  clear_path();
  if (training) training_phase = NEED_RESET;
  if (training)
  {
    try { save_q_table(q_file); }
    catch (const std::exception& ex) { RCLCPP_ERROR(get_logger(), "%s", ex.what()); }
  }
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
  previous_state.reset();
  pose_id = 1;
  path.reset();
  stop_moving();
  RCLCPP_INFO(this->get_logger(), "Path cleared, waiting for a new path");
}

double PathFollower::distance(PoseStamped point)
{
  return hypot(point.pose.position.x, point.pose.position.y);
}