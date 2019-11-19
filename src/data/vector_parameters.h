#ifndef BART_SRC_DATA_VECTOR_PARAMETERS_
#define BART_SRC_DATA_VECTOR_PARAMETERS_

#include <map>
#include <memory>

#include <deal.II/lac/petsc_parallel_vector.h>

namespace bart {

namespace data {

using FluxVector = dealii::PETScWrappers::MPI::Vector;
using ScalarFluxPtrs = std::map<int, std::unique_ptr<FluxVector>>;

} // namespace data

} // namespace bart

#endif // BART_SRC_DATA_VECTOR_PARAMETERS_