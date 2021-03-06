#ifndef BART_SRC_QUADRATURE_ORDINATE_H_
#define BART_SRC_QUADRATURE_ORDINATE_H_

#include "quadrature/ordinate_i.h"
#include "quadrature/quadrature_types.h"

namespace bart {

namespace quadrature {

/*! \brief Default implementation of Ordinate class.
 *
 * @tparam dim  spatial dimension.
 */
template <int dim>
class Ordinate : public OrdinateI<dim> {
 public:
  /*! \brief Default constructor, cartesian position is the origin. */
  Ordinate() { cartesian_position_.fill(0); }
  /*! \brief Constructor based on provided cartesian position. */
  explicit Ordinate(CartesianPosition<dim> position)
      : cartesian_position_(position.get()) {}
  virtual ~Ordinate() = default;

  Ordinate& set_cartesian_position(const CartesianPosition<dim> to_set) override {
    cartesian_position_ = to_set.get();
    return *this;
  }

  std::array<double, dim> cartesian_position() const override {
    return cartesian_position_;
  }

  dealii::Tensor<1, dim> cartesian_position_tensor() const override {
    dealii::Tensor<1, dim> return_tensor;
    for (int i = 0; i < dim; ++i)
      return_tensor[i] = cartesian_position_.at(i);
    return return_tensor;
  }

  bool operator==(const OrdinateI<dim>& rhs) const override {
    auto dynamic_rhs = dynamic_cast<const Ordinate<dim>&>(rhs);
    return cartesian_position_ == rhs.cartesian_position();
  }

  bool operator!=(const OrdinateI<dim>& rhs) const override {
    auto dynamic_rhs = dynamic_cast<const Ordinate<dim>&>(rhs);
    return cartesian_position_ != rhs.cartesian_position();
  }

  bool operator==(const std::array<double, dim> rhs) const override {
    return cartesian_position_ == rhs;
  };

  bool operator!=(const std::array<double, dim> rhs) const override {
    return cartesian_position_ != rhs;
  };

 private:
  std::array<double, dim> cartesian_position_;
};

} // namespace quadrature

} //namespace bart

#endif //BART_SRC_QUADRATURE_ORDINATE_H_
