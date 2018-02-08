#ifndef BART_TESTS_TEST_UTILITIES_H__
#define BART_TESTS_TEST_UTILITIES_H__

#include <fstream>

#include <deal.II/base/utilities.h>
#include <deal.II/base/logstream.h>

namespace testing {

std::ofstream deallogfile;
std::string deallogname;

void init_log ()
{
  deallogname = "output";
  deallogfile.open(deallogname.c_str());
  dealii::deallog.attach(deallogfile, false);
}

void collect_file (const char *filename)
{
  std::ifstream in (filename);

  while (in)
  {
    std::string s;
    std::getline(in, s);
    dealii::deallog.get_file_stream () << s << "\n";
  }

  in.close ();
  std::remove (filename);
}

struct MPILogInit
{
  MPILogInit ()
  {
    const unsigned int process_id = dealii::Utilities::MPI::this_mpi_process (
        MPI_COMM_WORLD);
    if (process_id==0)
    {
      if (!dealii::deallog.has_file())
      {
        deallogfile.open ("output");
        dealii::deallog.attach (deallogfile, false);
      }
    }
    else
    {
      deallogname = "output" + dealii::Utilities::int_to_string (process_id);
      deallogfile.open (deallogname.c_str());
      dealii::deallog.attach (deallogfile, false);
    }
    dealii::deallog.push ("Process "+
                          dealii::Utilities::int_to_string (process_id));
  }

  ~MPILogInit ()
  {
    const unsigned int process_id = dealii::Utilities::MPI::this_mpi_process (
        MPI_COMM_WORLD);
    const unsigned int n_proc = dealii::Utilities::MPI::n_mpi_processes (
        MPI_COMM_WORLD);

    //dealii::deallog.pop ();

    if (process_id!=0)
    {
      dealii::deallog.detach ();
      deallogfile.close ();
    }

    // wait until all the calls finish
    MPI_Barrier (MPI_COMM_WORLD);
    if (process_id==0)
    {
      // The following is a temporary fix for missing empty line for 0th output.
      deallogfile << std::endl;

      // The following block is to copy contents from output files from other
      // processors and paste them by the end of 0th output.
      for (unsigned int i=1; i<n_proc; ++i)
      {
        std::string filename = "output" + dealii::Utilities::int_to_string (i);
        collect_file (filename.c_str());
      }
    }
    MPI_Barrier (MPI_COMM_WORLD);
  }
};

}

#endif //BART_TESTS_TEST_UTILITIES_H__
