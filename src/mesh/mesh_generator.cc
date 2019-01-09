#include "mesh_generator.h"

#include <fstream>
#include <climits>

#include <deal.II/grid/tria.h>
#include <deal.II/grid/manifold_lib.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/grid/grid_in.h>

// note that prm here cannot be const because InitializeRelativePositionToIDMap
// invokes dealii::ParameterHandler::enter_subsection not marked const
template <int dim>
MeshGenerator<dim>::MeshGenerator (dealii::ParameterHandler &prm)
    : is_mesh_generated_(prm.get_bool("is mesh generated by deal.II")),
      have_reflective_bc_(prm.get_bool("have reflective boundary")),
      global_refinements_(prm.get_integer("uniform refinements")) {
  if (!is_mesh_generated_) {
    mesh_filename_ = prm.get ("mesh file name");
  } else {
    is_mesh_pin_resolved_ = prm.get_bool ("is mesh pin-resolved");
    if (is_mesh_pin_resolved_) {
      rod_radius_ = prm.get_double ("fuel rod radius");
      rod_type_ = prm.get ("fuel rod triangulation type");
    }
    ProcessCoordinateInformation (prm);
    InitializeRelativePositionToIDMap (prm);
    PreprocessReflectiveBC (prm);
  }
}

template <int dim>
MeshGenerator<dim>::~MeshGenerator () {}

template <int dim>
void MeshGenerator<dim>::MakeGrid (dealii::Triangulation<dim> &tria) {
  if (is_mesh_generated_) {
    GenerateInitialGrid (tria);
    InitializeMaterialID (tria);
    SetupBoundaryIDs (tria);
    if (global_refinements_>0)
      GlobalRefine (tria);
  } else {
    dealii::GridIn<dim> gi;
    gi.attach_triangulation (tria);
    std::ifstream f(mesh_filename_);
    gi.read_msh (f);
  }
}

template <int dim>
void MeshGenerator<dim>::FuelRod2D (dealii::Triangulation<2> &tria,
    const dealii::Point<2> &p) {
  double r = (rod_type_=="simple") ? rod_radius_ : (0.6*rod_radius_);
  const double a=1. / (std::sqrt(2.0)) * r;
  std::vector<dealii::Point<2>> v={
      p + dealii::Point<2>(0, 0)*r,
      p + dealii::Point<2>(0, 1)*r,
      p + dealii::Point<2>(1, 1)*a,
      p + dealii::Point<2>(1, 0)*r,
      p + dealii::Point<2>(1, -1)*a,
      p + dealii::Point<2>(0, -1)*r,
      p + dealii::Point<2>(-1, -1)*a,
      p + dealii::Point<2>(-1, 0)*r,
      p + dealii::Point<2>(-1, 1)*a
  };
  // vertices per cell for the circle
  std::vector<std::vector<dealii::Point<2>>> ps={
      {v[8], v[0], v[1], v[2]},
      {v[0], v[4], v[2], v[3]},
      {v[6], v[5], v[0], v[4]},
      {v[7], v[6], v[8], v[0]}
  };
  // generate a quater of the circle
  dealii::GridGenerator::general_cell(tria, ps[0]);
  // generate the rest of the circle
  for (int i=1; i<4; ++i) {
    dealii::Triangulation<2> t;
    dealii::GridGenerator::general_cell (t, ps[i]);
    dealii::GridGenerator::merge_triangulations (tria, t, tria);
  }

  // for composite type rod, a ring should be generated outside the inner circle
  if (rod_type_=="composite") {
    dealii::Triangulation<2> t;
    dealii::GridGenerator::hyper_shell (t, p, r, rod_radius_, 8);
    dealii::GridGenerator::merge_triangulations (tria, t, tria);
  }
}

template <int dim>
void MeshGenerator<dim>::FuelPin2DGrid (dealii::Triangulation<2> &tria,
    const dealii::Point<2> &center_xy) {
  // generate a fuel rod at origin
  FuelRod2D (tria, dealii::Point<2>());

  // generate the rest part of a 2D pin
  dealii::Triangulation<2> tria_moderator;
  dealii::GridGenerator::hyper_cube_with_cylindrical_hole (tria_moderator,
      rod_radius_, 0.5*cell_size_all_dir_[0]);


  // merge these two parts to get a pin
  dealii::GridGenerator::merge_triangulations (tria, tria_moderator, tria);

  // shift the triangulation to the desired center
  dealii::GridTools::shift (dealii::Tensor<1,2>(center_xy), tria);
}

template <int dim>
void MeshGenerator<dim>::NonFuelPin2DGrid (dealii::Triangulation<2> &tria,
    const dealii::Point<2> &center_xy) {
  // generate 4 squares around origin with same side length of rod_radius_
  // diags are the lower_left-upper_right diagonal points of squares
  double r = 0.5 * cell_size_all_dir_[0];
  std::vector<std::pair<dealii::Point<2>, dealii::Point<2>>> diags = {
      {dealii::Point<2>(-r, 0), dealii::Point<2>(0, r)},
      {dealii::Point<2>(0,0), dealii::Point<2>(r,r)},
      {dealii::Point<2>(-r, -r), dealii::Point<2>(0, 0)},
      {dealii::Point<2>(0,-r), dealii::Point<2>(r,0)}
  };
  // generate the first subcell
  dealii::GridGenerator::hyper_rectangle (tria,
      diags[0].first, diags[0].second);
  // generate the rest and merge together
  for (int i=1; i<4; ++i) {
    dealii::Triangulation<2> t;
    dealii::GridGenerator::hyper_rectangle (t, diags[i].first, diags[i].second);
    dealii::GridGenerator::merge_triangulations (tria, t, tria);
  }

  // shift the triangulation to the destination
  dealii::GridTools::shift (center_xy, tria);
}

template <>
void MeshGenerator<1>::GenerateInitialUnstructGrid (
    dealii::Triangulation<1> &) {}

template <>
void MeshGenerator<2>::GenerateInitialUnstructGrid (
    dealii::Triangulation<2> &tria) {
  // generate two basic pin models at lower left corner of the x-y plane
  dealii::Triangulation<2> t_fuel, t_moderator;
  double length = cell_size_all_dir_[0];
  FuelPin2DGrid (t_fuel, dealii::Point<2>(0.5*length, 0.5*length));
  NonFuelPin2DGrid (t_moderator, dealii::Point<2>(0.5*length, 0.5*length));

  // create a local triangulation and modify it for the first time
  dealii::Triangulation<2> t_loc;

  // modify the rest of the domain
  for (int ix=0; ix<ncell_per_dir_[0]; ++ix)
    for (int iy=0; iy<ncell_per_dir_[1]; ++iy) {
      std::vector<int> rel_pos = {ix, iy};
      auto fuel_id = relative_position_to_fuel_id_[rel_pos];
      // the first cell
      if (ix==0 && iy==0) {
        if (fuel_id<0)
          t_loc.copy_triangulation (t_moderator);
        else
          t_loc.copy_triangulation (t_fuel);
      } else {
        dealii::Triangulation<2> t_tmp;
        if (fuel_id<0)
          t_tmp.copy_triangulation (t_moderator);
        else
          t_tmp.copy_triangulation (t_fuel);

        dealii::GridTools::shift (
            dealii::Tensor<1, 2>(dealii::Point<2>(ix*length, iy*length)), t_tmp);
        dealii::GridGenerator::merge_triangulations (t_loc, t_tmp, t_loc);
      }
    }
  tria.copy_triangulation (t_loc);
}

template <>
void MeshGenerator<3>::GenerateInitialUnstructGrid (
    dealii::Triangulation<3> &tria) {
  // generate two basic pin models at lower left corner of the x-y plane
  dealii::Triangulation<2> t_fuel, t_moderator;
  double length = cell_size_all_dir_[0];
  FuelPin2DGrid (t_fuel, dealii::Point<2>(0.5*length, 0.5*length));
  NonFuelPin2DGrid (t_moderator, dealii::Point<2>(0.5*length, 0.5*length));

  // create a local triangulation and modify it for the first time
  dealii::Triangulation<2> t_loc;
  // modify the rest of the domain
  for (int ix=0; ix<ncell_per_dir_[0]; ++ix) {
    for (int iy=0; iy<ncell_per_dir_[1]; ++iy) {
      int fuel_id = -1;
      for (int iz=0; iz<ncell_per_dir_[2]; ++iz) {
        std::vector<int> rel_pos = {ix, iy, iz};
        if (relative_position_to_fuel_id_[rel_pos]>=0) {
          fuel_id = 1;
          curved_blocks.insert(std::vector<int>{ix, iy});
          break;
        }
      }
      // the first cell
      if (ix==0 && iy==0) {
        if (fuel_id<0)
          t_loc.copy_triangulation (t_moderator);
        else
          t_loc.copy_triangulation (t_fuel);
      } else {
        dealii::Triangulation<2> t_tmp;
        if (fuel_id<0)
          t_tmp.copy_triangulation (t_moderator);
        else
          t_tmp.copy_triangulation (t_fuel);

        dealii::GridTools::shift (
            dealii::Tensor<1, 2>(dealii::Point<2>(ix*length, iy*length)), t_tmp);
        dealii::GridGenerator::merge_triangulations (t_loc, t_tmp, t_loc);
      }
    }
  }
  // extrude 2d to 3d
  dealii::Triangulation<3> t_3d;
  dealii::GridGenerator::extrude_triangulation (
      t_loc, ncell_per_dir_[2]+1, axis_max_values_[2], t_3d);
  tria.copy_triangulation (t_3d);
}

template <int dim>
void MeshGenerator<dim>::GenerateInitialGrid (dealii::Triangulation<dim> &tria) {
  if (is_mesh_pin_resolved_) {
    AssertThrow(dim>1,
        dealii::ExcMessage("unstructured mesh is only valid for multi-D"))
    GenerateInitialUnstructGrid (tria);
  } else {
  // Note that this function is only suitable to
  // generate a hyper rectangle, which is a line
  // in 1D, a rectangle in 2D and a rectangular
  // cuboid in 3D

  // Construction of such a rectangle requires
  dealii::Point<dim> origin;
  dealii::Point<dim> diagonal;
  switch (dim) {
    case 1: {
      diagonal[0] = axis_max_values_[0];
      break;
    }

    case 2: {
      diagonal[0] = axis_max_values_[0];
      diagonal[1] = axis_max_values_[1];
      break;
    }

    case 3: {
      diagonal[0] = axis_max_values_[0];
      diagonal[1] = axis_max_values_[1];
      diagonal[2] = axis_max_values_[2];
      break;
    }

    default:
      break;
  }
  std::vector<unsigned int> tmp(ncell_per_dir_.begin(), ncell_per_dir_.end());
  dealii::GridGenerator::subdivided_hyper_rectangle (tria, tmp,
      origin, diagonal);
  }
}

template <int dim>
void MeshGenerator<dim>::InitializeMaterialID (
    dealii::Triangulation<dim> &tria) {
  AssertThrow (is_mesh_generated_==true,
      dealii::ExcMessage("mesh read in have to have boundary ids associated"));
  for (typename dealii::Triangulation<dim>::active_cell_iterator
       cell=tria.begin_active(); cell!=tria.end(); ++cell)
    if (cell->is_locally_owned()) {
      dealii::Point<dim> center(cell->center());
      std::vector<int> relative_position;
      GetCellRelativePosition(center, relative_position);
      int material_id = relative_position_to_id_[relative_position];

      if (is_mesh_pin_resolved_) {
        AssertThrow(dim>1,
            dealii::ExcMessage("unstructured mesh is not supported in 1D"));
        int fuel_id = relative_position_to_fuel_id_[relative_position];
        if (fuel_id<0) {
          // if it's not fuel pin, set material id
          cell->set_material_id(material_id);
        } else {
          AssertThrow (material_id!=fuel_id,
              dealii::ExcMessage("fuel rod must have different id than moderator"));
          // get pin center on xy plane
          double ctr_x, ctr_y;
          ctr_x = (0.5+relative_position[0])*cell_size_all_dir_[0];
          ctr_y = (0.5+relative_position[1])*cell_size_all_dir_[1];
          dealii::Point<dim> pin_center;
          pin_center[0]=ctr_x, pin_center[1]=ctr_y;
          if (dim==3) pin_center[2] = center[2];

          if (center.distance(pin_center)<rod_radius_) {
            // within pin, set id to fuel id
            cell->set_material_id(fuel_id);
          } else {
            // outside pin, set id to material id
            cell->set_material_id(material_id);
          }
        }
      } else {
        // structured mesh, directly set id
        cell->set_material_id (material_id);
      }
    }
}

template <int dim>
void MeshGenerator<dim>::GetCellRelativePosition (
    const dealii::Point<dim> &center,
    std::vector<int> &relative_position) {
  if (dim>=1) {
    relative_position.push_back(
        static_cast<int> (center[0] / cell_size_all_dir_[0]));
    if (dim>=2) {
      relative_position.push_back(
          static_cast<int> (center[1] / cell_size_all_dir_[1]));
      if (dim==3)
        relative_position.push_back(
            static_cast<int> (center[2] / cell_size_all_dir_[2]));
    }
  }
}

template <int dim>
void MeshGenerator<dim>::SetupBoundaryIDs
(dealii::Triangulation<dim> &tria)
{
  AssertThrow (is_mesh_generated_==true,
      dealii::ExcMessage("mesh read in have to have boundary ids associated"));
  AssertThrow (axis_max_values_.size()==dim,
      dealii::ExcMessage("number of entries axis max values should be dimension"));

  for (typename dealii::Triangulation<dim>::active_cell_iterator
       cell=tria.begin_active(); cell!=tria.end(); ++cell)
    if (cell->is_locally_owned())
      for (int fn=0; fn<dealii::GeometryInfo<dim>::faces_per_cell; ++fn)
        if (cell->at_boundary(fn)) {
          dealii::Point<dim> ct = cell->face(fn)->center();
          // x-axis boundaries
          if (std::fabs(ct[0])<1.0e-14)
            cell->face(fn)->set_boundary_id (0);
          else if (std::fabs(ct[0]-axis_max_values_[0])<1.0e-14)
            cell->face(fn)->set_boundary_id (1);

          // 2D and 3D boundaries
          if (dim>1) {
            // y-axis boundaries
            if (std::fabs(ct[1])<1.0e-14)
              cell->face(fn)->set_boundary_id (2);
            else if (std::fabs(ct[1]-axis_max_values_[1])<1.0e-14)
              cell->face(fn)->set_boundary_id (3);

            // z-axis boundaries for 3D only
            if (dim==3) {
              if (std::fabs(ct[2])<1.0e-14)
                cell->face(fn)->set_boundary_id (4);
              else if (std::fabs(ct[2]-axis_max_values_[2])<1.0e-14)
                cell->face(fn)->set_boundary_id (5);
            }
          }
        }
}

template <int dim>
void MeshGenerator<dim>::ProcessCoordinateInformation (
    const dealii::ParameterHandler &prm) {
  // max values for all axis
  std::vector<std::string> strings = dealii::Utilities::split_string_list (
      prm.get ("x, y, z max values of boundary locations"));
  AssertThrow (strings.size()>=dim,
      dealii::ExcMessage("Number of axis max values must be no less than dimension"));
  for (int i=0; i<dim; ++i)
    axis_max_values_.push_back (std::atof (strings[i].c_str()));

  // read in number of cells and get cell sizes along axes
  strings = dealii::Utilities::split_string_list (
      prm.get ("number of cells for x, y, z directions"));
  AssertThrow (strings.size()>=dim,
               dealii::ExcMessage ("Entries for numbers of cells must be no less than dimension"));
  std::vector<int> cells_per_dir;
  std::vector<std::vector<double> > spacings;
  for (int d=0; d<dim; ++d)
  {
    ncell_per_dir_.push_back (std::atoi (strings[d].c_str ()));
    cell_size_all_dir_.push_back (axis_max_values_[d]/ncell_per_dir_[d]);
  }
}

template <int dim>
void MeshGenerator<dim>::PreprocessReflectiveBC (
    const dealii::ParameterHandler &prm) {
  if (have_reflective_bc_) {
    std::map<std::string, int> bd_names_to_id {{"xmin",0}, {"xmax",1},
        {"ymin",2}, {"ymax",3}, {"zmin",4}, {"zmax",5}};
    std::vector<std::string> strings = dealii::Utilities::split_string_list (
        prm.get ("reflective boundary names"));
    AssertThrow (strings.size()>0,
        dealii::ExcMessage("reflective boundary names have to be entered"));
    std::set<int> tmp;
    for (int i=0; i<strings.size (); ++i) {
      AssertThrow(bd_names_to_id.find(strings[i])!=bd_names_to_id.end(),
          dealii::ExcMessage("Invalid reflective boundary name: use xmin, xmax, etc."));
      tmp.insert (bd_names_to_id[strings[i]]);
    }
    auto it = tmp.begin ();
    std::ostringstream os;
    os << "No valid reflective boundary name for " << dim << "D";
    AssertThrow(*it<2*dim,
                dealii::ExcMessage(os.str()));
    for (int i=0; i<2*dim; ++i) {
      if (tmp.count (i))
        is_reflective_bc_[i] = true;
      else
        is_reflective_bc_[i] = false;
    }
  }
}

template <int dim>
void MeshGenerator<dim>::InitializeRelativePositionToIDMap (
    dealii::ParameterHandler &prm) {
  prm.enter_subsection ("material ID map");
  {
    int ncell_z = dim==3?ncell_per_dir_[2]:1;
    int ncell_y = dim>=2?ncell_per_dir_[1]:1;
    int ncell_x = ncell_per_dir_[0];
    std::string id_fname = prm.get ("material id file name");
    std::ifstream in (id_fname);
    std::string line;
    int ct = 0;
    if (in.is_open ()) {
      while (std::getline (in, line)) {
        int y = ct % ncell_y;
        int z = ct / ncell_y;
        std::vector<std::string> strings =
            dealii::Utilities::split_string_list (line, ' ');
        AssertThrow (strings.size()==ncell_x,
            dealii::ExcMessage("Entries of material ID per row must be ncell_x"));
        for (int x=0; x<ncell_x; ++x) {
          std::vector<int> tmp = {x};
          if (dim>1) tmp.push_back (y);
          if (dim>2) tmp.push_back (z);
          relative_position_to_id_[tmp] = std::atoi (strings[x].c_str());
        }
        ct += 1;
      }
      AssertThrow (ct==ncell_y*ncell_z,
          dealii::ExcMessage("Number of y, z ID entries are not correct"));
      in.close ();
    }
    if (is_mesh_pin_resolved_) {
      std::string fuel_id_name = prm.get ("fuel pin material id file name");
      std::ifstream in_fuel (fuel_id_name);
      ct = 0;
      if (in_fuel.is_open ()) {
        while (std::getline (in_fuel, line)) {
          int y = ct % ncell_y;
          int z = ct / ncell_y;
          std::vector<std::string> strings =
              dealii::Utilities::split_string_list (line, ' ');
          AssertThrow (strings.size()==ncell_x,
              dealii::ExcMessage("Entries of material ID per row must be ncell_x"));
          for (int x=0; x<ncell_x; ++x) {
            std::vector<int> tmp = {x};
            if (dim>1) tmp.push_back (y);
            if (dim>2) tmp.push_back (z);
            // Note: material has to be stored using Hash table to BST
            relative_position_to_fuel_id_[tmp] = std::atoi (strings[x].c_str());
          }
          ct += 1;
        }
        AssertThrow (ct==ncell_y*ncell_z,
            dealii::ExcMessage("Number of y, z fuel ID entries are not correct"));
        in_fuel.close ();
      }
    }
  }
  prm.leave_subsection ();
}

template <>
void MeshGenerator<2>::SetManifoldsAndRefine (dealii::Triangulation<2> &tria) {
  // declare manifold as cylinder surfaces around axis parallel to z-axis
  std::map<std::vector<int>, dealii::SphericalManifold<2>*> rod_surfaces;
  //std::vector<dealii::SphericalManifold<2>*> rod_surfaces;
  for (int x=0; x<ncell_per_dir_[0]; ++x)
    for (int y=0; y<ncell_per_dir_[1]; ++y) {
      std::vector<int> t = {x, y};
      auto fuel_id = relative_position_to_fuel_id_[t];
      if (fuel_id>=0) {

        dealii::Point<2> ctr((0.5+x)*cell_size_all_dir_[0],
                             (0.5+y)*cell_size_all_dir_[1]);
        rod_surfaces[t] = new dealii::SphericalManifold<2> (ctr);
        tria.set_manifold (10+x+y*ncell_per_dir_[0], *rod_surfaces[t]);
      }
    }

  for (typename dealii::Triangulation<2>::active_cell_iterator
       cell=tria.begin_active(); cell!=tria.end(); ++cell)
    if (cell->is_locally_owned()) {
      std::vector<int> relative_position;
      GetCellRelativePosition (cell->center(), relative_position);
      auto fuel_id = relative_position_to_fuel_id_[relative_position];

      // only set manifolds for rod cells having cladding surface
      if (fuel_id>=0) {
        // fuel pin center on x-y plane
        dealii::Point<2> pin_ctr_xy (
            (0.5+relative_position[0])*cell_size_all_dir_[0],
            (0.5+relative_position[1])*cell_size_all_dir_[1]);
        dealii::Point<2> cell_ctr_xy (cell->center()[0], cell->center()[1]);
        if (cell_ctr_xy.distance(pin_ctr_xy)<rod_radius_) {
          int x=relative_position[0], y=relative_position[1];
          for (int vn=0; vn<dealii::GeometryInfo<2>::vertices_per_cell; ++vn) {
            dealii::Point<2> vertex_xy(cell->vertex(vn)[0], cell->vertex(vn)[1]);
            double dist = vertex_xy.distance(pin_ctr_xy);
            if (std::fabs(dist-rod_radius_)<1.0e-14) {
              cell->set_all_manifold_ids (10+x+y*ncell_per_dir_[0]);
              break;
            }
          }
        }

      }
    }
  // perform the refinement
  tria.refine_global (global_refinements_);

  // reset the manifolds to avoid error from deal.II design defect of manifold
  tria.set_manifold (INT_MAX);
}

template <>
void MeshGenerator<3>::SetManifoldsAndRefine (dealii::Triangulation<3> &tria) {
  // declare manifold as cylinder surfaces around axis parallel to z-axis
  std::map<std::vector<int>, dealii::CylindricalManifold<3>*> curved_surfaces;
  for (int ix=0; ix<ncell_per_dir_[0]; ++ix)
    for (int iy=0; iy<ncell_per_dir_[1]; ++iy) {
      std::vector<int> rel_pos = {ix, iy};
      if (curved_blocks.find(rel_pos)!=curved_blocks.end()) {
        dealii::Point<3> pt_on_axis ((0.5+ix)*cell_size_all_dir_[0],
            (0.5+iy)*cell_size_all_dir_[1] ,0);
        curved_surfaces[rel_pos] =
            new dealii::CylindricalManifold<3> (dealii::Point<3>(0,0,1.),
                pt_on_axis);
        tria.set_manifold (10+ix+iy*ncell_per_dir_[0],
            *curved_surfaces[rel_pos]);
      }
    }

  for (typename dealii::Triangulation<3>::active_cell_iterator
       cell=tria.begin_active(); cell!=tria.end(); ++cell)
    if (cell->is_locally_owned()) {
      //bool dist = cell->center().distance(cell->barycenter())>1.0e-15
      if (cell->center().distance(cell->barycenter())>1.0e-15) {
        std::vector<int> rel_pos;
        GetCellRelativePosition (cell->center(), rel_pos);
        dealii::Point<3> pin_ctr((0.5+rel_pos[0])*cell_size_all_dir_[0],
            (0.5+rel_pos[1])*cell_size_all_dir_[1],
            cell->center()[2]);
        if (pin_ctr.distance(cell->center())<rod_radius_)
          cell->set_all_manifold_ids(10+rel_pos[0]+rel_pos[1]*ncell_per_dir_[0]);
      }
    }

  // perform the refinement
  tria.refine_global (global_refinements_);
  // reset the manifolds to avoid error from deal.II design defect of manifold
  tria.set_manifold (INT_MAX);
}

template <>
void MeshGenerator<1>::GlobalRefine (dealii::Triangulation<1>& tria) {
  tria.refine_global (global_refinements_);
}

template <>
void MeshGenerator<2>::GlobalRefine (dealii::Triangulation<2>& tria) {
  if (is_mesh_pin_resolved_)
    // set manifolds for fuel rod surfaces and refine
    SetManifoldsAndRefine (tria);
  else
    tria.refine_global (global_refinements_);
}

template <>
void MeshGenerator<3>::GlobalRefine (dealii::Triangulation<3>& tria) {
  if (is_mesh_pin_resolved_)
    // set manifolds for fuel rod surfaces and refine
    SetManifoldsAndRefine (tria);
  else
    tria.refine_global (global_refinements_);
}

template <int dim>
std::unordered_map<int, bool>
MeshGenerator<dim>::GetReflectiveBCMap () {
  return is_reflective_bc_;
}

template <int dim>
int MeshGenerator<dim>::GetUniformRefinement () {
  return global_refinements_;
}

template class MeshGenerator<1>;
template class MeshGenerator<2>;
template class MeshGenerator<3>;
