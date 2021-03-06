#include "material_protobuf.h"

MaterialProtobuf::MaterialProtobuf(const std::unordered_map<int, Material>& materials,
                                   bool is_eigen_problem,
                                   bool do_nda,
                                   int number_of_groups,
                                   int number_of_materials)
    : materials_(materials),
      is_eigen_problem_(is_eigen_problem),
      do_nda_(do_nda),
      n_group_(number_of_groups),
      n_material_(number_of_materials) {

  if (is_eigen_problem) {
    for (const std::pair<int, Material>& mat_pair : materials_) {
      const int& id = mat_pair.first;
      const Material& material = mat_pair.second;

      if (material.is_fissionable())
        fissile_ids_.emplace(id);
    }
  }
  
  CheckFissileIDs();
  PopulateFissileMap(); // generates is_material_fissile_, which is used by CheckValidEach and PopulateData

  CheckNumberOfMaterials();
  CheckValidEach();
  CheckNumberOfGroups();
  CheckConsistent();
  PopulateData();
}

MaterialProtobuf::MaterialProtobuf(
    const std::unordered_map<int, std::string>& material_filename_map,
    bool is_eigen_problem,
    bool do_nda,
    int number_of_groups,
    int number_of_materials)
    : MaterialProtobuf(ParseMaterials(material_filename_map),
                       is_eigen_problem, do_nda, number_of_groups,
                       number_of_materials) {}

MaterialProtobuf::MaterialProtobuf(dealii::ParameterHandler& prm)
    : MaterialProtobuf(
        ParseMaterials(ReadMaterialFileNames(prm)),
        prm.get_bool("do eigenvalue calculations"),
        prm.get_bool("do nda"),
        prm.get_integer("number of groups"),
        prm.get_integer("number of materials")) {}

std::unordered_map<int, std::string> MaterialProtobuf::ReadMaterialFileNames(dealii::ParameterHandler& prm) {
  std::unordered_map<int, std::string> result;
  prm.enter_subsection("material ID map");
  const std::vector<std::string> pair_strings =
    dealii::Utilities::split_string_list(prm.get("material id file name map"), ",");

  for (const std::string& pair_string : pair_strings) {
    const std::vector<std::string> split_pair = dealii::Utilities::split_string_list(pair_string, ':');
    result[dealii::Utilities::string_to_int(split_pair[0])] = split_pair[1];
  }
  prm.leave_subsection();
  return result;
}

std::unordered_map<int, Material>
MaterialProtobuf::ParseMaterials(const std::unordered_map<int, std::string>& file_name_map) {
  /*
    Tries serialized format first and then readable text format if parsing as serialized format fails.
    This is because by default, parsing as text format will print errors if it doesn't work, but
    parsing as serialized format will not.
  */
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  std::unordered_map<int, Material> result;
  for (const std::pair<int, std::string>& id_file_pair : file_name_map) {
    result[id_file_pair.first] = Material();
    bool text_format_success = false;
    bool serialized_success = false;

    std::ifstream serialized_ifstream(id_file_pair.second, std::ios::binary);
    AssertThrow(serialized_ifstream.is_open(),
    FailedToFindMaterialFile(id_file_pair.second, id_file_pair.first));
    serialized_success = result[id_file_pair.first].ParseFromIstream(&serialized_ifstream);
    serialized_ifstream.close();

    if (!serialized_success) {
      std::ifstream text_format_ifstream(id_file_pair.second);
      AssertThrow(text_format_ifstream.is_open(),
      FailedToFindMaterialFile(id_file_pair.second, id_file_pair.first));
      // TextFormat::Parse requires different stream type
      google::protobuf::io::IstreamInputStream zero_copy_stream(&text_format_ifstream);
      text_format_success = google::protobuf::TextFormat::Parse(&zero_copy_stream, &result[id_file_pair.first]);
      text_format_ifstream.close();
    }

    AssertThrow(text_format_success || serialized_success,
      FailedToParseMaterialFile(id_file_pair.second, id_file_pair.first));
  }
  return result;
}

void MaterialProtobuf::PopulateFissileMap() {
  for (const std::pair<int, Material>& mat_pair : materials_) {
    const int& id = mat_pair.first;
    is_material_fissile_[id] = (fissile_ids_.count(id) > 0);
  }
}

void MaterialProtobuf::PopulateData() {
  /* 
    unordered maps for fixed source data will be left empty for an eigen problem and
    materials not labeled fissile will be excluded from the unordered_maps for fission data
    Q is assumed to be zero for all groups if not given in the Material
  */

  for (const std::pair<int, Material>& mat_pair : materials_) {
    const int& id = mat_pair.first;
    const Material& material = mat_pair.second;
    const std::unordered_map<Material::VectorId, std::vector<double>, std::hash<int>> vector_props = GetVectorProperties(material);

    sigt_[id] = vector_props.at(Material::SIGMA_T);
    sigs_[id] = GetScatteringMatrix(material);
    if (vector_props.count(Material::DIFFUSION_COEFF) > 0) {
      diffusion_coef_[id] = vector_props.at(Material::DIFFUSION_COEFF);
    } else {
      diffusion_coef_[id] = std::vector<double>(n_group_, 0);
    }

    if (is_material_fissile_[id]) {
      chi_[id] = vector_props.at(Material::CHI);
      nusigf_[id] = vector_props.at(Material::NU_SIG_F);
    }

    if (!is_eigen_problem_) {
      if (vector_props.count(Material::Q) > 0) {
        q_[id] = vector_props.at(Material::Q);
      }
      else {
        q_[id] = std::vector<double>(n_group_, 0);
      }
    }
  }

  for (const int& id : fissile_ids_) {
    dealii::FullMatrix<double> nusigf_column_matrix(n_group_, 1, nusigf_.at(id).data());
    dealii::FullMatrix<double> chi_row_matrix(1, n_group_, chi_.at(id).data());
    chi_nusigf_[id] = dealii::FullMatrix<double>(n_group_, n_group_);
    // next line is equivalent to chi_nusigf_[id] = nusigf_column_matrix * chi_row_matrix
    nusigf_column_matrix.mmult(chi_nusigf_[id], chi_row_matrix);
  }

  for (const std::pair<int, std::vector<double>>& sigt_pair : sigt_) {
    inv_sigt_[sigt_pair.first] = sigt_pair.second;
    for (double& val : inv_sigt_.at(sigt_pair.first)) {
      val = 1 / val;
    }
  }

  for (const std::pair<int, std::vector<double>>& q_pair : q_) {
    q_per_ster_[q_pair.first] = q_pair.second;
    for (double& val : q_per_ster_.at(q_pair.first)) {
      val *= bconst::kInvFourPi;
    }
  }

  for (const std::pair<int, dealii::FullMatrix<double>>& sigs_pair : sigs_) {
    sigs_per_ster_[sigs_pair.first] = sigs_pair.second;
    sigs_per_ster_[sigs_pair.first] *= bconst::kInvFourPi;
  }

  for (const std::pair<int, dealii::FullMatrix<double>>& chi_nusigf_pair : chi_nusigf_) {
    chi_nusigf_per_ster_[chi_nusigf_pair.first] = chi_nusigf_pair.second;
    chi_nusigf_per_ster_[chi_nusigf_pair.first] *= bconst::kInvFourPi;
  }
}

void MaterialProtobuf::CheckFissileIDs() const {
  AssertThrow(!(is_eigen_problem_ && fissile_ids_.empty()), NoFissileIDs());
}

void MaterialProtobuf::CheckNumberOfMaterials() const {
  AssertThrow(n_material_ >= 0 && (unsigned int)(n_material_) == materials_.size(),
    WrongNumberOfMaterials(materials_.size(), n_material_));
}

void MaterialProtobuf::CheckNumberOfGroups() const {
  for (const std::pair<int, Material>& mat_pair : materials_) {
    const Material& mat = mat_pair.second;
    AssertThrow(n_group_ >= 0 && (unsigned int)(n_group_) == mat.number_of_groups(),
      WrongNumberOfGroups(CombinedName(mat_pair), mat.number_of_groups(), n_group_));
  }
}

void MaterialProtobuf::CheckValid(const Material& material,
                                    const bool require_fission_data /* = false */,
                                    std::string name /* = "" */) {
  /* 
    calls GetVectorProperties and GetScatteringMatrix, which can throw exceptions, 
    but the order of checks is such that they shouldn't
  */
  if (name == "") {
    name = CombinedName(material);
  }

  // MultipleDefinition
  std::unordered_set<Material::VectorId, std::hash<int>> vector_ids_in_material;

  for (const Material_VectorProperty& vec_prop : material.vector_property()) {
    AssertThrow(vector_ids_in_material.count(vec_prop.id()) == 0 || vec_prop.id() == Material::UNKNOWN_VECTOR,
      MultipleDefinition(name, Material_VectorId_descriptor()->value(vec_prop.id())->name()));

    vector_ids_in_material.insert(vec_prop.id());
  }

  std::unordered_set<Material::MatrixId, std::hash<int>> matrix_ids_in_material;

  for (const Material_MatrixProperty& mat_prop : material.matrix_property()) {
    AssertThrow(matrix_ids_in_material.count(mat_prop.id()) == 0 || mat_prop.id() == Material::UNKNOWN_MATRIX,
      MultipleDefinition(name, Material_MatrixId_descriptor()->value(mat_prop.id())->name()));

    matrix_ids_in_material.insert(mat_prop.id());
  }

  // MissingProperty
  std::unordered_set<Material::VectorId, std::hash<int>> required_vector_props = {Material::ENERGY_GROUPS, Material::SIGMA_T};

  if (require_fission_data) {
    required_vector_props.insert(Material::NU_SIG_F);
    required_vector_props.insert(Material::CHI);
  }

  std::unordered_map<Material::VectorId, std::vector<double>, std::hash<int>> vector_props = GetVectorProperties(material);

  for (Material::VectorId id : required_vector_props) {
    AssertThrow(vector_props.count(id) == 1,
      MissingProperty(name, Material_VectorId_descriptor()->value(id)->name()));
  }

  AssertThrow(matrix_ids_in_material.count(Material::SIGMA_S) > 0,
    MissingProperty(name, Material_MatrixId_descriptor()->value(Material::SIGMA_S)->name()));

  // WrongNumberOfValues
  if (vector_props.count(Material::Q) > 0) {
    required_vector_props.insert(Material::Q);
  }

  if (vector_props.count(Material::DIFFUSION_COEFF) > 0) {
    required_vector_props.insert(Material::DIFFUSION_COEFF);
  } 
  const unsigned int& n = material.number_of_groups();
  std::unordered_map<Material::VectorId, unsigned int, std::hash<int>> required_count =
   {{Material::ENERGY_GROUPS, n+1}, {Material::SIGMA_T, n},
    {Material::Q, n}, {Material::NU_SIG_F, n}, {Material::CHI, n},
    {Material::DIFFUSION_COEFF, n}};

  for (Material::VectorId id : required_vector_props) {
    AssertThrow(vector_props.at(id).size() == required_count.at(id),
      WrongNumberOfValues(name, Material_VectorId_descriptor()->value(id)->name(),
        vector_props.at(id).size(), n));
  }

  for (const Material_MatrixProperty& mat_prop : material.matrix_property()) {
    if (mat_prop.id() == Material::SIGMA_S) {
      AssertThrow((unsigned int)mat_prop.value().size() == n*n,
          WrongNumberOfValues(name, Material_MatrixId_descriptor()->value(Material::SIGMA_S)->name(),
            mat_prop.value().size(), material.number_of_groups()));
    }
  }

  // WrongSign
  const std::unordered_set<Material::VectorId, std::hash<int>> required_non_negative = required_vector_props;
  for (Material::VectorId id : required_non_negative) {
    for (unsigned int i = 0; i < vector_props.at(id).size(); ++i) {
      AssertThrow(vector_props.at(id)[i] >= 0,
        WrongSign(name, Material_VectorId_descriptor()->value(id)->name(),
          vector_props.at(id)[i], i));
    }
  }

  const dealii::FullMatrix<double> scat_mat = GetScatteringMatrix(material);
  for (dealii::FullMatrix<double>::const_iterator iter = scat_mat.begin(); iter < scat_mat.end(); ++iter) {
    AssertThrow(iter->value() >= 0,
      WrongSign(name, Material_MatrixId_descriptor()->value(Material::SIGMA_S)->name(),
        iter->value(), iter->row()*n + iter->column()));
  }

  // EnergyGroupBoundariesNotSorted
  const std::vector<double> energies = vector_props.at(Material::ENERGY_GROUPS);
  for (std::vector<double>::const_iterator i = energies.cbegin() + 1; i < energies.cend() - 1; ++i) {
    // require that every set of three consecutive values is in either strictly ascending or strictly descending order
    AssertThrow((*(i-1) > *i && *i > *(i+1)) || (*(i-1) < *i && *i < *(i+1)),
      EnergyGroupBoundariesNotSorted(name, *(i-1), *i, *(i+1)));
  }

  // ChiDoesNotSumToOne
  if (required_vector_props.count(Material::CHI) > 0) {
    // allow a maximum error from 1 of two units in the last place
    AssertThrow(abs(butil::PreciseSum(vector_props.at(Material::CHI)) - 1) <= 2*std::numeric_limits<double>::epsilon(),
                ChiDoesNotSumToOne(name, butil::PreciseSum(vector_props.at(Material::CHI))));
  }
}

void MaterialProtobuf::CheckValidEach() const {
  for (const std::pair<int, Material>& mat_pair : materials_) {
    CheckValid(mat_pair.second, is_material_fissile_.at(mat_pair.first), CombinedName(mat_pair));
  }
}

void MaterialProtobuf::CheckConsistent(const std::unordered_map<int, Material>& materials) {
  if (materials.empty()) {
    return;
  }

  const std::vector<double> first_mat_energies = GetVectorProperty(materials.cbegin()->second, Material::ENERGY_GROUPS);

  for (std::unordered_map<int, Material>::const_iterator iter = materials.cbegin(); iter != materials.cend(); ++iter) {
    AssertThrow(materials.cbegin()->second.number_of_groups() == iter->second.number_of_groups(),
      NumberOfGroupsMismatch(CombinedName(*materials.cbegin()), CombinedName(*iter)));

    const std::vector<double> this_mat_energies = GetVectorProperty(iter->second, Material::ENERGY_GROUPS);

    AssertThrow(first_mat_energies.size() == this_mat_energies.size(),
      EnergyGroupsMismatch(CombinedName(*materials.cbegin()), CombinedName(*iter),
        std::min(first_mat_energies.size(), this_mat_energies.size())+1));

    for (unsigned int i = 0; i < first_mat_energies.size(); ++i) {
      AssertThrow(first_mat_energies[i] == this_mat_energies[i],
        EnergyGroupsMismatch(CombinedName(*materials.cbegin()), CombinedName(*iter), i));
    }
  }
}

void MaterialProtobuf::CheckConsistent() const {
  CheckConsistent(materials_);
}

std::string MaterialProtobuf::CombinedName(const std::pair<int, Material>& id_material_pair,
                                             const std::string& delimiter /* = "|" */) {
  const std::string q = "\"";
  const std::string& d = delimiter;
  const Material& mat = id_material_pair.second;
  std::string format = "number" + d + "full_name" + d + "abbreviation" + d + "id";
  std::string info = std::to_string(id_material_pair.first) + d + q+mat.full_name()+q +
                     d + q+mat.abbreviation()+q + d + q+mat.id()+q;
  return format + " = " + info;
}

std::string MaterialProtobuf::CombinedName(const Material& material,
                                             const std::string& delimiter /* = "|" */) {
  const std::string q = "\"";
  const std::string& d = delimiter;
  std::string format = "full_name" + d + "abbreviation" + d + "id";
  std::string info = q+material.full_name()+q + d + q+material.abbreviation()+q + d + q+material.id()+q;
  return format + " = " + info;
}

std::unordered_map<Material::VectorId, std::vector<double>, std::hash<int>>
MaterialProtobuf::GetVectorProperties(const Material& material) {
  std::unordered_map<Material::VectorId, std::vector<double>, std::hash<int>> result;
  for (const Material_VectorProperty& vec_prop : material.vector_property()) {
    AssertThrow(result.count(vec_prop.id()) == 0 || vec_prop.id() == Material::UNKNOWN_VECTOR,
      MultipleDefinition(CombinedName(material), Material_VectorId_descriptor()->value(vec_prop.id())->name()));

    std::vector<double> new_vector(vec_prop.value().cbegin(), vec_prop.value().cend());
    result[vec_prop.id()] = new_vector;
  }
  return result;
}

std::vector<double> MaterialProtobuf::GetVectorProperty(const Material& material, Material::VectorId property_id) {
  for (const Material_VectorProperty& vec_prop : material.vector_property()) {
    if (vec_prop.id() == property_id) {
      return std::vector<double>(vec_prop.value().cbegin(), vec_prop.value().cend());
    }
  }
  return {};
}

dealii::FullMatrix<double> MaterialProtobuf::GetScatteringMatrix(const Material& material) {
  for (const Material_MatrixProperty& mat_prop : material.matrix_property()) {
    if (mat_prop.id() == Material::SIGMA_S) {
      std::vector<double> raw_values(mat_prop.value().cbegin(), mat_prop.value().cend());
      
      AssertThrow(raw_values.size() == material.number_of_groups()*material.number_of_groups(),
        WrongNumberOfValues(CombinedName(material), Material_MatrixId_descriptor()->value(Material::SIGMA_S)->name(),
          raw_values.size(), material.number_of_groups()));

      dealii::FullMatrix<double> scattering_matrix(material.number_of_groups(), material.number_of_groups());
      scattering_matrix.fill(raw_values.data());
      return scattering_matrix;
    }
  }
  return dealii::FullMatrix<double>();
}

std::unordered_map<int, bool> MaterialProtobuf::GetFissileIDMap() const {
  return is_material_fissile_;
}

std::unordered_map<int, std::vector<double>> MaterialProtobuf::GetDiffusionCoef()
    const {
  return diffusion_coef_;
}
  

std::unordered_map<int, std::vector<double>> MaterialProtobuf::GetSigT() const {
  return sigt_;
}

std::unordered_map<int, std::vector<double>> MaterialProtobuf::GetInvSigT() const {
  return inv_sigt_;
}

std::unordered_map<int, std::vector<double>> MaterialProtobuf::GetQ() const {
  return q_;
}

std::unordered_map<int, std::vector<double>> MaterialProtobuf::GetQPerSter() const {
  return q_per_ster_;
}

std::unordered_map<int, std::vector<double>> MaterialProtobuf::GetNuSigF() const {
  return nusigf_;
}

std::unordered_map<int, dealii::FullMatrix<double>> MaterialProtobuf::GetSigS() const {
  return sigs_;
}

std::unordered_map<int, dealii::FullMatrix<double>> MaterialProtobuf::GetSigSPerSter() const {
  return sigs_per_ster_;
}

std::unordered_map<int, dealii::FullMatrix<double>> MaterialProtobuf::GetChiNuSigF() const {
  return chi_nusigf_;
}

std::unordered_map<int, dealii::FullMatrix<double>> MaterialProtobuf::GetChiNuSigFPerSter() const {
  return chi_nusigf_per_ster_;
}
