#include "modules/filter/filter.h"
namespace coop {
namespace v2x {
class MeanFilter:public Filter {
 public:
  MeanFilter();
  explicit MeanFilter(const int window_size);
  ~MeanFilter();
  void Propagate();
  void Update(const double measurement);

 private:
 bool initialized_ = false;
 int window_size_;
};
}  // namespace v2x
}  // namespace coop