#ifndef BART_SRC_CONVERGENCE_STATUS_H_
#define BART_SRC_CONVERGENCE_STATUS_H_

#include <optional>

namespace bart {

namespace convergence {

/*! Contains the status of a convergence check */
struct Status {
  int iteration_number = 0;
  int max_iterations = 100;
  bool is_complete = false;
  std::optional<int> failed_index = std::nullopt;
  std::optional<double> delta = std::nullopt;

  bool operator==(const Status rhs) const {
    if (iteration_number != rhs.iteration_number) {
      return false;
    } else if (max_iterations != rhs.max_iterations) {
      return false;
    } else if (is_complete != rhs.is_complete) {
      return false;
    } else if (failed_index != rhs.failed_index) {
      return false;
    } else if (delta != rhs.delta) {
      return false;
    }
    return true;
  }
};




} // namespace convergence

} // namespace bart

#endif // BART_SRC_CONVERGENCE_STATUS_H_    
