#pragma once
#include <memory>

#include "hmm.h"
namespace V2I {
class trellis {
 public:
  trellis();
  ~trellis();
  void viterbi();

  void simple_viterbi();

 private:
  std::shared_ptr<model> hmm;
};
}  // namespace V2I