// Header file containing core template functions to calculate statistic values for Part Average Test in ME

#pragma once
#include <algorithm>
#include <cmath>
#if __cplusplus >= 201703L
#include <execution>
#endif
#include <vector>

#include "Eigen/Dense"

inline size_t getBenardNumber(const double val, const size_t length) noexcept {
    return static_cast<size_t>(std::floor(val * (static_cast<double>(length) + 0.4) + 0.3));
}


inline double getBenardValue(const size_t num, const size_t length) noexcept {
    return (static_cast<double>(num) - 0.3) / (static_cast<double>(length) + 0.4);
}


template<typename Real>
inline std::vector<Real> quantile(std::vector<Real> &&inData, const std::vector<Real> &probs) noexcept {
    if (inData.empty()) {
        return {};
    }

    if (1 == inData.size()) {
        return std::vector<Real>(1, inData[0]);
    }

#if __cplusplus >= 201703L
    std::sort(std::execution::par_unseq, inData.begin(), inData.end());
#else
    std::sort(inData.begin(), inData.end());
#endif
    std::vector<Real> quantiles;
    quantiles.reserve(probs.size());
    for (Real prob : probs) {
        size_t length = inData.size();
        size_t left = getBenardNumber(prob, length);
        size_t right = left + 1;

        double datLeft = inData[left - 1];
        double datRight = inData[right - 1];
        double B1 = getBenardValue(left, length);
        double B2 = getBenardValue(right, length);
        double quantile = datLeft + (datRight - datLeft) * (prob - B1) / (B2 - B1);

        quantiles.push_back(static_cast<Real>(quantile));
    }

    return quantiles;
}

template <typename Derived>
inline Eigen::Matrix<typename Derived::Scalar, Derived::ColsAtCompileTime, Derived::ColsAtCompileTime>
covariance( const Eigen::DenseBase<Derived>& x ) noexcept {
  typedef typename Eigen::internal::plain_row_type<Derived>::type RowVectorType;
  const RowVectorType x_mean = x.colwise().mean();

  return ((x.rowwise() - x_mean).matrix().transpose() * (x.rowwise() - x_mean).matrix()) / (x.rows()-1);
}


/// @brief vec<R> -> vec<R>: calculates PAT per vector of parameters value
/// @tparam Real floating point type (float or double)
/// @param values column of parameter values
/// @return vec<R>: ordered vector of calculated PAT values, same size as input
template <typename Real>
std::vector<Real> calculateUD(const std::vector<Real> &values) {
  if (values.empty()) {
    return {};
  }

  std::vector<Real> copy = values;
  std::vector<Real> quantiles = quantile(std::move(copy), {0.2, 0.5, 0.8});

  double B20 = quantiles[0];
  double Median = quantiles[1];
  double B80 = quantiles[2];
  double S = (B80 - B20) / 1.683242467;

  std::vector<Real> out;
  for (const double x : values) {
    double pat = (x - Median) / S;
    out.push_back(static_cast<Real>(pat));
  }

  return out;
}

template <typename Real>
std::vector<std::vector<Real>> calculateUD(const std::vector<std::vector<Real>> &values) {
  if (values.empty()) {
    return {};
  }

  using std::vector;

  std::vector<std::vector<Real>> out;
  out.reserve(values.size());

  for (const vector<Real> &v : values) {
    std::vector<Real> &&patValues = calculateUD(v);
    out.emplace_back(patValues);
  }

  return out;
}



/// @brief vec<vec<R>> -> vec<R>: calculates Mahalanobis distances per row
/// @tparam Real floating point type (float or double)
/// @param values matrix in which columns are parameter values
/// @return 
template <typename Real>
std::vector<Real> calculateMD(const std::vector<std::vector<Real>> &values) {
  if (values.empty()) {
    return {};
  }

  using namespace Eigen;
  using MatrixR = Matrix<Real, Dynamic, Dynamic>;
  using VectorR = Matrix<Real, Dynamic, 1>;

  const size_t vsize = values[0].size();
  const size_t msize = values.size();
  MatrixR mat(vsize, msize);

  for(size_t i = 0; i < msize; ++i) {
    mat.col(i) = Map<const VectorR, Unaligned>(values[i].data(), vsize);
  }

  const VectorR means = mat.colwise().mean();
  const MatrixR cov = covariance(mat);
  const LDLT<MatrixR> ldlt = cov.ldlt();

  std::vector<Real> out;
  out.reserve(vsize);
  for (size_t i = 0; i < vsize; ++i) {
    const VectorR v = mat.row(i).transpose() - means;
    Real d2 = v.dot(ldlt.solve(v));
    Real md = std::sqrt(std::max(Real{0.0}, d2));
    out.push_back(md);
  }

  return out;
}
