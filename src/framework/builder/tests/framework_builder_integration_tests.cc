#include <deal.II/fe/fe_q.h>

#include "framework/builder/framework_builder.h"

// Instantiated concerete classes
#include "convergence/reporter/mpi_noisy.h"
#include "convergence/final_checker_or_n.h"
#include "convergence/parameters/single_parameter_checker.h"
#include "convergence/moments/single_moment_checker_i.h"
#include "data/cross_sections.h"
#include "domain/finite_element/finite_element_gaussian.h"
#include "domain/definition.h"
#include "eigenvalue/k_effective/updater_via_fission_source.h"
#include "formulation/scalar/diffusion.h"
#include "formulation/angular/self_adjoint_angular_flux.h"
#include "formulation/updater/saaf_updater.h"
#include "formulation/updater/diffusion_updater.h"
#include "formulation/stamper.h"
#include "iteration/outer/outer_power_iteration.h"
#include "quadrature/calculators/scalar_moment.h"
#include "quadrature/calculators/spherical_harmonic_zeroth_moment.h"
#include "quadrature/quadrature_set.h"
#include "solver/gmres.h"
#include "solver/group/single_group_solver.h"
#include "system/solution/mpi_group_angular_solution.h"
#include "iteration/initializer/initialize_fixed_terms_once.h"
#include "iteration/group/group_source_iteration.h"
#include "system/system_types.h"

// Mock objects
#include "convergence/reporter/tests/mpi_mock.h"
#include "convergence/tests/final_checker_mock.h"
#include "domain/tests/definition_mock.h"
#include "domain/finite_element/tests/finite_element_mock.h"
#include "eigenvalue/k_effective/tests/k_effective_updater_mock.h"
#include "formulation/angular/tests/self_adjoint_angular_flux_mock.h"
#include "formulation/scalar/tests/diffusion_mock.h"
#include "formulation/tests/stamper_mock.h"
#include "formulation/updater/tests/scattering_source_updater_mock.h"
#include "formulation/updater/tests/fission_source_updater_mock.h"
#include "formulation/updater/tests/fixed_updater_mock.h"
#include "iteration/group/tests/group_solve_iteration_mock.h"
#include "material/tests/mock_material.h"
#include "problem/tests/parameters_mock.h"
#include "formulation/updater/tests/fixed_updater_mock.h"
#include "quadrature/tests/quadrature_set_mock.h"
#include "quadrature/calculators/tests/spherical_harmonic_moments_mock.h"
#include "solver/group/tests/single_group_solver_mock.h"
#include "system/solution/tests/mpi_group_angular_solution_mock.h"
#include "utility/reporter/tests/basic_reporter_mock.h"

#include "test_helpers/gmock_wrapper.h"
#include "test_helpers/test_helper_functions.h"

namespace {

using namespace bart;

using ::testing::Return, ::testing::NiceMock, ::testing::DoDefault;
using ::testing::WhenDynamicCastTo, ::testing::NotNull;
using ::testing::HasSubstr, ::testing::_;
using ::testing::ReturnRef, ::testing::A;

using ::testing::AtLeast;

template <typename DimensionWrapper>
class FrameworkBuilderIntegrationTest : public ::testing::Test {
 public:
  static constexpr int dim = DimensionWrapper::value;

  using FrameworkBuilder = framework::builder::FrameworkBuilder<dim>;
  using ProblemParameters = NiceMock<problem::ParametersMock>;
  using Material = NiceMock<btest::MockMaterial>;

  // Mock object types
  using ConvergenceReporterType = convergence::reporter::MpiMock;
  using DiffusionFormulationType = formulation::scalar::DiffusionMock<dim>;
  using DomainType = domain::DefinitionMock<dim>;
  using FiniteElementType = domain::finite_element::FiniteElementMock<dim>;
  using FissionSourceUpdaterType = formulation::updater::FissionSourceUpdaterMock;
  using GroupSolutionType = system::solution::MPIGroupAngularSolutionMock;
  using GroupSolveIterationType = iteration::group::GroupSolveIterationMock;
  using KEffectiveUpdaterType = eigenvalue::k_effective::K_EffectiveUpdaterMock;
  using MomentCalculatorType = quadrature::calculators::SphericalHarmonicMomentsMock;
  using MomentConvergenceCheckerType = convergence::FinalCheckerMock<bart::system::moments::MomentVector>;
  using ParameterConvergenceCheckerType = convergence::FinalCheckerMock<double>;
  using QuadratureSetType = quadrature::QuadratureSetMock<dim>;
  using SAAFFormulationType = formulation::angular::SelfAdjointAngularFluxMock<dim>;
  using ScatteringSourceUpdaterType = formulation::updater::ScatteringSourceUpdaterMock;
  using SingleGroupSolverType = solver::group::SingleGroupSolverMock;
  using StamperType = formulation::StamperMock<dim>;
  using ReporterType = NiceMock<utility::reporter::BasicReporterMock>;

  FrameworkBuilderIntegrationTest()
      : mock_material() {}

  std::unique_ptr<FrameworkBuilder> test_builder_ptr_;
  ProblemParameters parameters;
  Material mock_material;
  std::shared_ptr<ReporterType> mock_reporter_ptr_;

  // Various mock objects to be used
  std::shared_ptr<ConvergenceReporterType> convergence_reporter_sptr_;
  std::shared_ptr<data::CrossSections> cross_sections_sptr_;
  std::unique_ptr<DiffusionFormulationType> diffusion_formulation_uptr_;
  std::shared_ptr<DomainType> domain_sptr_;
  std::shared_ptr<FiniteElementType> finite_element_sptr_;
  std::shared_ptr<FissionSourceUpdaterType> fission_source_updater_sptr_;
  std::shared_ptr<GroupSolutionType> group_solution_sptr_;
  std::unique_ptr<GroupSolveIterationType> group_solve_iteration_uptr_;
  std::unique_ptr<KEffectiveUpdaterType> k_effective_updater_uptr_;
  std::unique_ptr<MomentCalculatorType> moment_calculator_uptr_;
  std::unique_ptr<MomentConvergenceCheckerType> moment_convergence_checker_uptr_;
  std::unique_ptr<ParameterConvergenceCheckerType> parameter_convergence_checker_uptr_;
  std::shared_ptr<QuadratureSetType> quadrature_set_sptr_;
  std::unique_ptr<SAAFFormulationType> saaf_formulation_uptr_;
  std::shared_ptr<ScatteringSourceUpdaterType> scattering_source_updater_sptr_;
  std::unique_ptr<SingleGroupSolverType> single_group_solver_uptr_;
  std::unique_ptr<StamperType> stamper_uptr_;

  // Test Parameters
  const int polynomial_degree = 2;
  std::vector<double> spatial_max;
  std::vector<int> n_cells;
  const int n_energy_groups = 3;
  std::array<int, 4> dofs_per_cell_by_dim_{1, 3, 9, 27};

  void SetUp() override;
};

template <typename DimensionWrapper>
void FrameworkBuilderIntegrationTest<DimensionWrapper>::SetUp() {
  convergence_reporter_sptr_ = std::make_shared<ConvergenceReporterType>();
  cross_sections_sptr_ = std::make_shared<data::CrossSections>(mock_material);
  diffusion_formulation_uptr_ =
      std::move(std::make_unique<DiffusionFormulationType>());
  domain_sptr_ = std::make_shared<DomainType>();
  finite_element_sptr_ = std::make_shared<FiniteElementType>();
  fission_source_updater_sptr_ = std::make_shared<FissionSourceUpdaterType>();
  group_solution_sptr_ = std::make_shared<GroupSolutionType>();
  group_solve_iteration_uptr_  = std::move(
      std::make_unique<GroupSolveIterationType>());
  k_effective_updater_uptr_ = std::move(
      std::make_unique<KEffectiveUpdaterType>());
  moment_calculator_uptr_ = std::move(std::make_unique<MomentCalculatorType>());
  moment_convergence_checker_uptr_ =
      std::move(std::make_unique<MomentConvergenceCheckerType>());
  parameter_convergence_checker_uptr_ = std::move(
      std::make_unique<ParameterConvergenceCheckerType>());
  quadrature_set_sptr_ = std::make_shared<QuadratureSetType>();
  saaf_formulation_uptr_ = std::move(std::make_unique<SAAFFormulationType>());
  scattering_source_updater_sptr_ = std::make_shared<ScatteringSourceUpdaterType>();
  stamper_uptr_ = std::move(std::make_unique<StamperType>());
  mock_reporter_ptr_ = std::make_shared<ReporterType>();
  single_group_solver_uptr_ = std::move(std::make_unique<SingleGroupSolverType>());

  test_builder_ptr_ = std::move(std::make_unique<FrameworkBuilder>(mock_reporter_ptr_));

  for (int i = 0; i < this->dim; ++i) {
    spatial_max.push_back(10);
    n_cells.push_back(2);
  }

  std::map<problem::Boundary, bool> reflective_bcs{
      {problem::Boundary::kXMin, true},
      {problem::Boundary::kXMax, true},
      {problem::Boundary::kYMin, false},
      {problem::Boundary::kYMax, false},
      {problem::Boundary::kZMin, false},
      {problem::Boundary::kZMax, false},
  };

  ON_CALL(parameters, NEnergyGroups())
      .WillByDefault(Return(n_energy_groups));
  ON_CALL(parameters, NCells())
      .WillByDefault(Return(n_cells));
  ON_CALL(parameters, SpatialMax())
      .WillByDefault(Return(spatial_max));
  ON_CALL(parameters, FEPolynomialDegree())
      .WillByDefault(Return(polynomial_degree));
  ON_CALL(parameters, TransportModel())
      .WillByDefault(Return(problem::EquationType::kDiffusion));
  ON_CALL(parameters, ReflectiveBoundary())
      .WillByDefault(Return(reflective_bcs));
  ON_CALL(*mock_reporter_ptr_, Instream(A<const std::string&>()))
      .WillByDefault(ReturnRef(*mock_reporter_ptr_));
  ON_CALL(*mock_reporter_ptr_, Instream(A<utility::reporter::Color>()))
      .WillByDefault(ReturnRef(*mock_reporter_ptr_));
}

TYPED_TEST_CASE(FrameworkBuilderIntegrationTest,
                bart::testing::AllDimensions);

TYPED_TEST(FrameworkBuilderIntegrationTest, Getters) {
  auto reporter_ptr = this->test_builder_ptr_->reporter_ptr();
  EXPECT_THAT(reporter_ptr,
              WhenDynamicCastTo<utility::reporter::BasicReporterMock*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildConvergenceReporterTest) {
  using ExpectedType = convergence::reporter::MpiNoisy;
  auto convergence_reporter_ptr = this->test_builder_ptr_->BuildConvergenceReporter();
  EXPECT_THAT(convergence_reporter_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildDiffusionFormulationTest) {
  constexpr int dim = this->dim;

  auto finite_element_ptr =
      std::make_shared<domain::finite_element::FiniteElementMock<dim>>();
  auto cross_sections_ptr =
      std::make_shared<data::CrossSections>(this->mock_material);

  EXPECT_CALL(*finite_element_ptr, dofs_per_cell());
  EXPECT_CALL(*finite_element_ptr, n_cell_quad_pts());
  EXPECT_CALL(*finite_element_ptr, n_face_quad_pts());

  auto diffusion_formulation_ptr = this->test_builder_ptr_->BuildDiffusionFormulation(
      finite_element_ptr, cross_sections_ptr);

  using ExpectedType = formulation::scalar::Diffusion<dim>;
  EXPECT_THAT(diffusion_formulation_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildDiffusionUpdaterPointers) {
  constexpr int dim = this->dim;
  using ExpectedType = formulation::updater::DiffusionUpdater<dim>;
  auto updater_struct = this->test_builder_ptr_->BuildUpdaterPointers(
      std::move(this->diffusion_formulation_uptr_),
      std::move(this->stamper_uptr_));
  EXPECT_THAT(updater_struct.fixed_updater_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
  EXPECT_THAT(updater_struct.scattering_source_updater_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
  EXPECT_THAT(updater_struct.fission_source_updater_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildSAAFUpdaterPointers) {
  constexpr int dim = this->dim;
  using ExpectedType = formulation::updater::SAAFUpdater<dim>;
  auto updater_struct = this->test_builder_ptr_->BuildUpdaterPointers(
      std::move(this->saaf_formulation_uptr_),
      std::move(this->stamper_uptr_),
      this->quadrature_set_sptr_);
  EXPECT_THAT(updater_struct.fixed_updater_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
  EXPECT_THAT(updater_struct.scattering_source_updater_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
  EXPECT_THAT(updater_struct.fission_source_updater_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildDomainTest) {
  constexpr int dim = this->dim;
  auto finite_element_ptr =
      std::make_shared<NiceMock<domain::finite_element::FiniteElementMock<dim>>>();

  EXPECT_CALL(this->parameters, NCells())
      .WillOnce(DoDefault());
  EXPECT_CALL(this->parameters, SpatialMax())
      .WillOnce(DoDefault());

  auto test_domain_ptr = this->test_builder_ptr_->BuildDomain(
      this->parameters, finite_element_ptr, "1 1 2 2");

  using ExpectedType = domain::Definition<this->dim>;

  EXPECT_THAT(test_domain_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildGroupSourceIterationTest) {
  using ExpectedType = iteration::group::GroupSourceIteration<this->dim>;

  auto source_iteration_ptr = this->test_builder_ptr_->BuildGroupSolveIteration(
      std::move(this->single_group_solver_uptr_),
      std::move(this->moment_convergence_checker_uptr_),
      std::move(this->moment_calculator_uptr_),
      this->group_solution_sptr_,
      this->scattering_source_updater_sptr_,
      this->convergence_reporter_sptr_);
  EXPECT_THAT(source_iteration_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildGroupSolution) {
  using ExpectedType = system::solution::MPIGroupAngularSolution;
  const int n_angles = bart::test_helpers::RandomDouble(1, 10);
  auto group_solution_ptr = this->test_builder_ptr_->BuildGroupSolution(n_angles);

  ASSERT_THAT(group_solution_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
  EXPECT_EQ(n_angles, group_solution_ptr->total_angles());
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildFiniteElementTest) {
  constexpr int dim = this->dim;
  EXPECT_CALL(this->parameters, FEPolynomialDegree())
      .WillOnce(DoDefault());

  using ExpectedType = domain::finite_element::FiniteElementGaussian<dim>;

  auto finite_element_ptr = this->test_builder_ptr_->BuildFiniteElement(this->parameters);
  auto gaussian_ptr = dynamic_cast<ExpectedType*>(finite_element_ptr.get());

  EXPECT_NE(gaussian_ptr, nullptr);
  EXPECT_EQ(finite_element_ptr->polynomial_degree(), this->polynomial_degree);
  auto dealii_finite_element_ptr = dynamic_cast<dealii::FE_Q<dim>*>(
      finite_element_ptr->finite_element());
  EXPECT_NE(dealii_finite_element_ptr, nullptr);
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildKeffectiveUpdater) {
  using ExpectedType = eigenvalue::k_effective::UpdaterViaFissionSource;
  EXPECT_CALL(*this->finite_element_sptr_, n_cell_quad_pts())
      .WillOnce(Return(10));
  auto k_effective_updater_ptr = this->test_builder_ptr_->BuildKEffectiveUpdater(
      this->finite_element_sptr_,
      this->cross_sections_sptr_,
      this->domain_sptr_);
  EXPECT_THAT(k_effective_updater_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildMomentCalculatorScalar) {
  using ExpectedType = quadrature::calculators::ScalarMoment;

  auto moment_calculator_ptr = this->test_builder_ptr_->BuildMomentCalculator();
  ASSERT_THAT(moment_calculator_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildMomentCalculatorScalarQuadSet) {
  using ExpectedType = quadrature::calculators::ScalarMoment;
  using Implementation = quadrature::MomentCalculatorImpl;
  auto moment_calculator_ptr = this->test_builder_ptr_->BuildMomentCalculator(
      this->quadrature_set_sptr_, Implementation::kScalarMoment);
  ASSERT_THAT(moment_calculator_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BulidMomentCalculatorAngular) {
  constexpr int dim = this->dim;
  using ExpectedType = quadrature::calculators::SphericalHarmonicZerothMoment<dim>;

  auto moment_calculator_ptr = this->test_builder_ptr_->BuildMomentCalculator(
      this->quadrature_set_sptr_);
  ASSERT_THAT(moment_calculator_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildPowerIterationTest) {
  auto power_iteration_ptr = this->test_builder_ptr_->BuildOuterIteration(
      std::move(this->group_solve_iteration_uptr_),
      std::move(this->parameter_convergence_checker_uptr_),
      std::move(this->k_effective_updater_uptr_),
      this->fission_source_updater_sptr_,
      this->convergence_reporter_sptr_);
  using ExpectedType = iteration::outer::OuterPowerIteration;
  ASSERT_THAT(power_iteration_ptr.get(),
                  WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildLSAngularQuadratureSet) {
  constexpr int dim = this->dim;
  const int order = 4;
  EXPECT_CALL(this->parameters, AngularQuad())
      .WillOnce(Return(problem::AngularQuadType::kLevelSymmetricGaussian));
  EXPECT_CALL(this->parameters, AngularQuadOrder())
      .WillOnce(Return(order));

  if (dim == 3) {
    using ExpectedType = quadrature::QuadratureSet<dim>;
    auto quadrature_set = this->test_builder_ptr_->BuildQuadratureSet(this->parameters);
    ASSERT_NE(nullptr, quadrature_set);
    ASSERT_NE(nullptr, dynamic_cast<ExpectedType*>(quadrature_set.get()));
    EXPECT_EQ(quadrature_set->size(), order * (order + 2));
  } else {
    EXPECT_ANY_THROW({
      auto quadrature_set = this->test_builder_ptr_->BuildQuadratureSet(this->parameters);
    });
  }
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildSingleGroupSolver) {
  using ExpectedType = solver::group::SingleGroupSolver;

  auto solver_ptr = this->test_builder_ptr_->BuildSingleGroupSolver(100, 1e-12);

  ASSERT_NE(nullptr, solver_ptr);

  auto dynamic_ptr = dynamic_cast<ExpectedType*>(solver_ptr.get());
  ASSERT_NE(nullptr, dynamic_ptr);

  using ExpectedLinearSolverType = solver::GMRES;

  auto linear_solver_ptr = dynamic_cast<ExpectedLinearSolverType*>(
      dynamic_ptr->linear_solver_ptr());

  ASSERT_NE(nullptr, linear_solver_ptr);
  EXPECT_EQ(linear_solver_ptr->convergence_tolerance(), 1e-12);
  EXPECT_EQ(linear_solver_ptr->max_iterations(), 100);
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildConvergenceChecker) {
  const double max_delta = 1e-4;
  const int max_iterations = 100;

  auto convergence_ptr =
      this->test_builder_ptr_->BuildParameterConvergenceChecker(
          max_delta,
          max_iterations);

  using ParameterConvergenceChecker = convergence::FinalCheckerOrN<double, convergence::parameters::SingleParameterChecker>;


  ASSERT_NE(convergence_ptr, nullptr);
  EXPECT_NE(nullptr,
            dynamic_cast<ParameterConvergenceChecker*>(convergence_ptr.get()));
  EXPECT_EQ(convergence_ptr->max_iterations(), max_iterations);

}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildMomentConvergenceChecker) {
  const double max_delta = 1e-4;
  const int max_iterations = 100;

  auto convergence_ptr =
      this->test_builder_ptr_->BuildMomentConvergenceChecker(
          max_delta,
          max_iterations);

  using ExpectedType =
  convergence::FinalCheckerOrN<system::moments::MomentVector,
                               convergence::moments::SingleMomentCheckerI>;

  EXPECT_THAT(convergence_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
  EXPECT_EQ(convergence_ptr->max_iterations(), max_iterations);

}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildSAAFFormulationTest) {
  constexpr int dim = this->dim;

  auto finite_element_ptr =
      std::make_shared<domain::finite_element::FiniteElementMock<dim>>();
  auto cross_sections_ptr =
      std::make_shared<data::CrossSections>(this->mock_material);
  auto quadrature_set_ptr =
      std::make_shared<quadrature::QuadratureSetMock<dim>>();

  EXPECT_CALL(*finite_element_ptr, dofs_per_cell());
  EXPECT_CALL(*finite_element_ptr, n_cell_quad_pts());
  EXPECT_CALL(*finite_element_ptr, n_face_quad_pts());

  auto saaf_formulation_ptr = this->test_builder_ptr_->BuildSAAFFormulation(
      finite_element_ptr, cross_sections_ptr, quadrature_set_ptr);

  using ExpectedType = formulation::angular::SelfAdjointAngularFlux<dim>;

  EXPECT_THAT(saaf_formulation_ptr.get(),
              WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

TYPED_TEST(FrameworkBuilderIntegrationTest, BuildStamper) {
  constexpr int dim = this->dim;

  auto domain_ptr = std::make_shared<domain::DefinitionMock<dim>>();

  using ExpectedType = formulation::Stamper<dim>;
  auto stamper_ptr = this->test_builder_ptr_->BuildStamper(domain_ptr);

  EXPECT_THAT(stamper_ptr.get(), WhenDynamicCastTo<ExpectedType*>(NotNull()));
}

/* ===== Non-dimensional tests =================================================
 * These tests instantiate classes and use depdent classes that do not have a
 * dimension template varaible and therefore only need to be run in a single
 * dimension.
*/


class FrameworkBuilderIntegrationNonDimTest
 : public FrameworkBuilderIntegrationTest<bart::testing::OneD> {};

TEST_F(FrameworkBuilderIntegrationNonDimTest, BuildInitializer) {
  auto fixed_updater_ptr =
      std::make_shared<formulation::updater::FixedUpdaterMock>();

  using ExpectedType = iteration::initializer::InitializeFixedTermsOnce;
  const int total_groups = bart::test_helpers::RandomDouble(1, 10);
  const int total_angles = total_groups + 1;

  auto initializer_ptr = this->test_builder_ptr_->BuildInitializer(fixed_updater_ptr,
                                                                  total_groups,
                                                                  total_angles);
  auto dynamic_ptr = dynamic_cast<ExpectedType*>(initializer_ptr.get());
  ASSERT_NE(initializer_ptr, nullptr);
  ASSERT_NE(dynamic_ptr, nullptr);
  EXPECT_EQ(dynamic_ptr->total_angles(), total_angles);
  EXPECT_EQ(dynamic_ptr->total_groups(), total_groups);
}

TEST_F(FrameworkBuilderIntegrationNonDimTest, BuildSystemDefaut) {

}

} // namespace
