#include "modules/filter/kalman_filter.h"
#include <iostream>
#include "modules/common/util/util.h"
namespace coop {
namespace v2x {
ExtendedKalmanFilter::ExtendedKalmanFilter() : inited_(false) {}
ExtendedKalmanFilter::~ExtendedKalmanFilter() {}
void ExtendedKalmanFilter::Init() {
  // other value should be changed in predict
  state_transition_matrix_.setIdentity();
  measure_matrix_ << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,0, 0, 0, 1;
  variance_.setIdentity();
  measure_noise_.setIdentity();
  // measure_noise_(0,0)=1.55*1.55;  // sigmaX*sigmaX
  // measure_noise_(1,1)=1.55*1.55;  // sigmaY*sigmaY
  measure_noise_(0,0)=0.95*0.95;  // sigmaX*sigmaX
  measure_noise_(1,1)=0.95*0.95;  // sigmaY*sigmaY
  measure_noise_(2,2)=(0.55)*(0.55); // sigmaTheta*sigmaTheta
  measure_noise_(3,3)=(10*DEG2RAD)*(10*DEG2RAD); // sigmaTheta*sigmaTheta
  // measure_noise_(3,3)=(1.6*DEG2RAD)*(1.6*DEG2RAD);
  process_noise_.setIdentity();
  // process_noise_=process_noise_*(0.4*0.4);
  process_noise_(0,0)=0.2*0.2;
  process_noise_(1,1)=0.2*0.2;
  process_noise_(2,2)=0.2*0.2;
  process_noise_(3,3)=(3*DEG2RAD)*(3*DEG2RAD);
  // process_noise_*=0.074;
  inited_ = false;
}

void ExtendedKalmanFilter::Init(Eigen::VectorXd x) {
  Init();
  // state_ << x(0), x(1), 0, x(2);
  state_ << x(0), x(1), x(2), x(3);
  inited_ = true;
}
void ExtendedKalmanFilter::Predict(float delta_t) {
  if (inited_) {
    float sin_theta = static_cast<float>(std::sin(state_(3)));
    float cos_theta = static_cast<float>(std::cos(state_(3)));
    state_transition_matrix_(0, 2) = delta_t * cos_theta;
    state_transition_matrix_(0, 3) = -delta_t * state_(2) * sin_theta;
    state_transition_matrix_(1, 2) = delta_t * sin_theta;
    state_transition_matrix_(1, 3) = delta_t * state_(2) * cos_theta;
    state_(0) += state_(2) * cos_theta * delta_t;
    state_(1) += state_(2) * sin_theta * delta_t;
    //    No change
    //    state_(2) = state_(2);
    //    state_(3) = state_(3);
    variance_ = state_transition_matrix_ * variance_ *
                    state_transition_matrix_.transpose() +
                process_noise_;
  }
}
void ExtendedKalmanFilter::Correct(const Eigen::VectorXd &z) {
  if (inited_) {
    Eigen::Vector4d measure;
    measure << z[0], z[1], z[2],z[3];
    Eigen::Matrix4d cov =
        measure_matrix_ * variance_ * measure_matrix_.transpose() +
        measure_noise_;

    kalman_gain_ = variance_ * measure_matrix_.transpose() * cov.inverse();
    variance_ = variance_ - kalman_gain_ * measure_matrix_ * variance_;
    Eigen::Vector4d diff=measure - measure_matrix_ * state_;
    diff(3)=normalizeRad(diff(3));
    // state_ = state_ + kalman_gain_ * (measure - measure_matrix_ * state_);
    state_ = state_ + kalman_gain_ * (diff);

  } else {
    inited_ = true;
    state_ << z[0], z[1], z[2], z[3];
  }

  
}
Eigen::Vector4d ExtendedKalmanFilter::get_state() const { return state_; }

}  // namespace v2x
}  // namespace coop