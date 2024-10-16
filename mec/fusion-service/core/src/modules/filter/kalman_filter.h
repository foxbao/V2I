#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Dense>
#include "modules/filter/filter.h"
namespace coop {
namespace v2x {
class ExtendedKalmanFilter : public Filter {
 public:
  ExtendedKalmanFilter();
  ~ExtendedKalmanFilter();
  void Init();
  void Init(Eigen::VectorXd);
  void Predict(float delta_t);
  void Correct(const Eigen::VectorXd &z);

  // x,y,v,theta
  Eigen::Vector4d get_state() const;

  Eigen::Matrix4d variance_;
  Eigen::Matrix4d measure_noise_;
  Eigen::Matrix4d process_noise_;

 private:
  // X = (x,y,v,theta)'
  // x = x + v cos(theta)
  // y = y + v sin(theta)
  // v = v + noise
  // theta = theta + noise
  // z = (x,y,theta)'
  Eigen::Matrix4d state_transition_matrix_;

  Eigen::Matrix<double, 4, 1> state_;
  Eigen::Matrix<double, 4, 4> measure_matrix_;
  Eigen::Matrix<double, 4, 4> kalman_gain_;
  bool inited_;
};
}  // namespace v2x
}  // namespace coop