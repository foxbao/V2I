#include "modules/filter/mean_filter.h"
namespace coop {
namespace v2x {
MeanFilter::MeanFilter() {
    window_size_=4;
}
MeanFilter::MeanFilter(const int window_size) : window_size_(window_size) {
//   CHECK_GT(window_size_, 0);
//   CHECK_LE(window_size_, kMaxWindowSize);
  initialized_ = true;
}
MeanFilter::~MeanFilter() {}
void MeanFilter::Propagate() {}
void MeanFilter::Update(const double measurement) {}
}  // namespace v2x
}  // namespace coop