#include "domain/mesh_cartesian.h"

#include <algorithm>
#include <cstdlib>
#include <numeric>
#include <functional>
#include <vector>

#include <deal.II/base/point.h>
#include <deal.II/grid/tria.h>

#include "test_helpers/test_helper_functions.h"
#include "test_helpers/gmock_wrapper.h"

namespace  {

using namespace bart;

template <typename DimensionWrapper>
class DomainMeshCartesianTest : public ::testing::Test {
 protected:
  static constexpr int dim = DimensionWrapper::value;
};

TYPED_TEST_CASE(DomainMeshCartesianTest, bart::testing::AllDimensions);

TYPED_TEST(DomainMeshCartesianTest, FillTriangulationTest) {
  constexpr int dim = this->dim;

  std::vector<double> spatial_max{btest::RandomVector(dim, 0, 100)};
  std::vector<double> n_cells_double{btest::RandomVector(dim, 1, 20)};
  std::vector<int> n_cells{n_cells_double.begin(), n_cells_double.end()};

  int n_total_cells = std::accumulate(n_cells.begin(), n_cells.end(), 1,
                                      std::multiplies<int>());

  domain::MeshCartesian<dim> test_mesh(spatial_max, n_cells);
  dealii::Triangulation<dim> test_triangulation;

  test_mesh.FillTriangulation(test_triangulation);
  EXPECT_EQ(test_triangulation.n_cells(), n_total_cells);
  EXPECT_FALSE(test_mesh.has_material_mapping());
  for (auto const &cell : test_triangulation.active_cell_iterators()) {
    for (int i = 0; i < dim; ++i) {
      EXPECT_THAT(cell->extent_in_direction(i),
                  ::testing::DoubleNear(spatial_max[i]/n_cells[i], 1e-10));
    }
  }
}

TYPED_TEST(DomainMeshCartesianTest, BadSpatialSize) {
  constexpr int dim = this->dim;

  std::vector<std::vector<double>> spatial_maxes{
      {},
      btest::RandomVector(1, 0, 100),
      btest::RandomVector(2, 0, 100),
      btest::RandomVector(3, 0, 100),
      btest::RandomVector(4, 0, 100)};

  std::vector<std::vector<int>> n_cells{{}, {10}, {10, 20}, {10, 20, 30},
                                        {10, 20, 30, 40}};
  std::array<int, 2> i_values{-1, 1};

  for (const auto& i : i_values) {
    EXPECT_ANY_THROW({
                       domain::MeshCartesian<dim> test_mesh(spatial_maxes.at(dim + i),
                                                            n_cells.at(dim + i));
                     });
  }
}

TYPED_TEST(DomainMeshCartesianTest, SingleMaterialMapping) {
  constexpr int dim = this->dim;
  std::vector<double> spatial_max{btest::RandomVector(dim, 5, 20)};
  std::vector<double> n_cells_double{btest::RandomVector(dim, 5, 20)};
  std::vector<int> n_cells{n_cells_double.cbegin(),
                           n_cells_double.cend()};

  domain::MeshCartesian<dim> test_mesh(spatial_max, n_cells);
  std::string material_mapping{'1'};

  test_mesh.ParseMaterialMap(material_mapping);
  EXPECT_TRUE(test_mesh.has_material_mapping());

  // Random inner locations
  std::array<std::array<double, dim>, 10> test_locations;

  for (auto& location : test_locations) {
    for (int i = 0; i < dim; ++i) {
      location.at(i) = btest::RandomDouble(0, spatial_max.at(i));
    }

    EXPECT_EQ(test_mesh.GetMaterialID(location), 1);
  }

  // Edges and corners
  std::array<double, dim> test_location;

  std::array<double, 3> x_locations{0, spatial_max.at(0)/2, spatial_max.at(0)};
  std::vector<double> y_locations{};
  std::vector<double> z_locations{};

  if (dim > 1) {
    y_locations = {0, spatial_max.at(1)/2, spatial_max.at(1)};
    if (dim > 2) {
      z_locations = {0, spatial_max.at(2)/2, spatial_max.at(2)};
    }
  }

  for (const int x : x_locations) {
    test_location.at(0) = x;
    for (const int y : y_locations) {
      test_location.at(1) = y;
      for (const int z : z_locations) {
        test_location.at(2) = z;
      }
    }
    EXPECT_EQ(test_mesh.GetMaterialID(test_location), 1);
  }

}

TYPED_TEST(DomainMeshCartesianTest, MultipleMaterialMapping) {

}

} // namespace
