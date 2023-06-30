#pragma once
namespace V2I {
class ttc {
 public:
  ttc();
  ~ttc();
  double CalculateTTC(double X1, double X2, double v1,double v2,double l2=5);

 private:
};
}  // namespace V2I