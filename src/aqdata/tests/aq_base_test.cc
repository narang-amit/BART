#include "../aq_base.h"

#include <string>

#include "gtest/gtest.h"
#include "../../test_helpers/bart_test_helper.h"

class AQBaseTest : public ::testing::Test {
 protected:
  void SetUp ();

  template<int dim>
  void OutputAQ ();
  
  dealii::ParameterHandler prm;
};

void AQBaseTest::SetUp () {
  prm.declare_entry ("have reflective BC", "false",
                     dealii::Patterns::Bool(), "");
  prm.declare_entry ("angular quadrature order", "4",
                     dealii::Patterns::Integer (), "");
  prm.declare_entry ("angular quadrature name", "gl",
                     dealii::Patterns::Selection ("gl"), "");
  prm.declare_entry ("number of groups", "1", dealii::Patterns::Integer (), "");
  prm.declare_entry ("transport model", "regular",
                     dealii::Patterns::Selection("regular|ep"), "");
}

template<int dim>
void AQBaseTest::OutputAQ () {
  // Outputs AQData generated by MakeAQ to the deallog
  std::unique_ptr<AQBase<dim>> gl_ptr =
      std::unique_ptr<AQBase<dim>> (new AQBase<dim>(prm));
  //gl_ptr->MakeAQ ();
  gl_ptr->ProduceAQ();
  auto wi = gl_ptr->GetAQWeights ();
  auto omega_i = gl_ptr->GetAQDirs ();
  for (int i=0; i<wi.size(); ++i) {
    dealii::deallog << "Weight: " << wi[i] << "; Omega: ";
    for (int j=0; j<dim; ++j)
      dealii::deallog << omega_i[i][j] << " ";
    dealii::deallog << std::endl;
  }
}

TEST_F (AQBaseTest, AQBase1DProduceAQ) {
  std::string filename = "aq_base_1d";
  btest::GoldTestInit(filename); // Opens deal log
  OutputAQ<1>();

  btest::GoldTestRun(filename); // Closes deal log
}

TEST_F (AQBaseTest, AQBase1DEpProduceAQ) {
  std::string filename = "aq_base_1d_ep";
  prm.set ("transport model", "ep");
  btest::GoldTestInit(filename); // Opens deal log
  OutputAQ<1>();

  btest::GoldTestRun(filename); // Closes deal log
}

TEST_F (AQBaseTest, AQBaseBadDimProduceAQ) {
  AQBase<2> test_AQ(prm);
  ASSERT_THROW(test_AQ.ProduceAQ(), dealii::ExcMessage);
}




