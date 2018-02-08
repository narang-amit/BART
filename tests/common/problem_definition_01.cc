#include "../../src/common/problem_definition.h"
#include "../test_utilities.h"

void setup_parameters (dealii::ParameterHandler &prm)
{
  // set entry values for those without default input
  prm.set ("reflective boundary names", "xmin");
  prm.set ("x, y, z max values of boundary locations", "1.0, 2.0");
  prm.set ("number of cells for x, y, z directions", "1, 3");
}

void find_errors (dealii::ParameterHandler &prm)
{
  // dealii::ExcInternalError() is used to indicate expected
  // condition is not satisfied.
  AssertThrow (prm.get_integer("problem dimension")==2,
               dealii::ExcInternalError());
  AssertThrow (prm.get("transport model")=="none", dealii::ExcInternalError());
  AssertThrow (prm.get("HO linear solver name")=="cg",
               dealii::ExcInternalError());
  AssertThrow (prm.get("HO preconditioner name")=="amg",
               dealii::ExcInternalError());
  AssertThrow (prm.get_double("HO ssor factor")==1.0,
               dealii::ExcInternalError());
  AssertThrow (prm.get("NDA linear solver name")=="none",
               dealii::ExcInternalError());
  AssertThrow (prm.get("NDA preconditioner name")=="none",
               dealii::ExcInternalError());
  AssertThrow (prm.get_double("NDA ssor factor")==1.0,
               dealii::ExcInternalError());
  AssertThrow (prm.get("angular quadrature name")=="none",
               dealii::ExcInternalError());
  AssertThrow (prm.get_integer("angular quadrature order")==4,
               dealii::ExcInternalError());
  AssertThrow (prm.get_integer("number of groups")==1,
               dealii::ExcInternalError());
  AssertThrow (prm.get_integer("thermal group boundary")==0,
               dealii::ExcInternalError());
  AssertThrow (prm.get("spatial discretization")=="cfem",
               dealii::ExcInternalError());
  AssertThrow (prm.get_bool("do eigenvalue calculations")==false,
               dealii::ExcInternalError());
  AssertThrow (prm.get_bool("do NDA")==false, dealii::ExcInternalError());
  AssertThrow (prm.get_bool("have reflective BC")==false,
               dealii::ExcInternalError());
  AssertThrow (prm.get("reflective boundary names")=="xmin",
               dealii::ExcInternalError());
  AssertThrow (prm.get_integer("finite element polynomial degree")==1,
               dealii::ExcInternalError());
  AssertThrow (prm.get_integer("uniform refinements")==0,
               dealii::ExcInternalError());
  AssertThrow (prm.get("x, y, z max values of boundary locations")=="1.0, 2.0",
               dealii::ExcInternalError());
  AssertThrow (prm.get("number of cells for x, y, z directions")=="1, 3",
               dealii::ExcInternalError());
  AssertThrow (prm.get_integer("number of materials")==1,
               dealii::ExcInternalError());
  AssertThrow (prm.get_bool("do print angular quadrature info")==true,
               dealii::ExcInternalError());
  AssertThrow (prm.get_bool("is mesh generated by deal.II")==true,
               dealii::ExcInternalError());
  AssertThrow (prm.get("output file name base")=="solu",
               dealii::ExcInternalError());
  AssertThrow (prm.get("mesh file name")=="mesh.msh",
               dealii::ExcInternalError());
}

void test (dealii::ParameterHandler &prm)
{
  // purpose of this test is to see whether parameters
  // are parsed correctly
  ProblemDefinition::declare_parameters (prm);
  setup_parameters (prm);
  find_errors (prm);
  dealii::deallog << "OK" << std::endl;
}

int main ()
{
  dealii::ParameterHandler prm;
  
  testing::init_log ();
  
  test (prm);
  return 0;
}
