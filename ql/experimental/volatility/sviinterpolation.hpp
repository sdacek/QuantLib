/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file sviinterpolation.hpp
    \brief Svi interpolation interpolation between discrete points
*/

#ifndef quantlib_svi_interpolation_hpp
#define quantlib_svi_interpolation_hpp

#include <ql/experimental/volatility/noarbsvi.hpp>
#include <ql/experimental/volatility/svismilesection.hpp>
#include <ql/math/interpolations/xabrinterpolation.hpp>
#include <utility>

namespace QuantLib {

namespace detail {

inline void checkSviParameters(const Real a, const Real b, const Real sigma,
                               const Real rho, const Real m, const Time tte) {
    QL_REQUIRE(b >= 0.0, "b (" << b << ") must be non negative");
    QL_REQUIRE(std::fabs(rho) < 1.0, "rho (" << rho << ") must be in (-1,1)");
    QL_REQUIRE(sigma > 0.0, "sigma (" << sigma << ") must be positive");
    QL_REQUIRE(a + b * sigma * std::sqrt(1.0 - rho * rho) >= 0.0,
               "a + b sigma sqrt(1-rho^2) (a=" << a << ", b=" << b << ", sigma="
                                               << sigma << ", rho=" << rho
                                               << ") must be non negative");
    QL_REQUIRE(b * (1.0 + std::fabs(rho)) <= 4.0,
               "b(1+|rho|) must be less than or equal to 4, (b=" << b << ", rho=" << rho << ")");

}

inline Real sviTotalVariance(const Real a, const Real b, const Real sigma,
                             const Real rho, const Real m, const Real k) {
    return a +
           b * (rho * (k - m) + std::sqrt((k - m) * (k - m) + sigma * sigma));
}

typedef SviSmileSection SviWrapper;

struct SviSpecs {
    Size dimension() { return 5; }
    void defaultValues(std::vector<Real> &params,
                       std::vector<bool> &paramIsFixed, const Real &forward,
                       const Real expiryTime,
                       const std::vector<Real> &addParams) {
        if (params[2] == Null<Real>())
            params[2] = 0.1;
        if (params[3] == Null<Real>())
            params[3] = -0.4;
        if (params[4] == Null<Real>())
            params[4] = 0.0;
        if (params[1] == Null<Real>())
            params[1] = 2.0 / (1.0 + std::fabs(params[3]));
        if (params[0] == Null<Real>()) {
            params[0] = std::max(
                0.20 * 0.20 * expiryTime -
                    params[1] * (params[3] * (-params[4]) +
                                 std::sqrt((-params[4]) * (-params[4]) +
                                           params[2] * params[2])),
                -params[1] * params[2] *
                std::sqrt(1.0 - params[3] * params[3]) + eps1());
        }
    }
    void guess(Array &values, const std::vector<bool> &paramIsFixed,
               const Real &forward, const Real expiryTime,
               const std::vector<Real> &r, const std::vector<Real> &addParams) {
        Size j = 0;
        if (!paramIsFixed[2])
            values[2] = r[j++] + eps1();
        if (!paramIsFixed[3])
            values[3] = (2.0 * r[j++] - 1.0) * eps2();
        if (!paramIsFixed[4])
            values[4] = (2.0 * r[j++] - 1.0);
        if (!paramIsFixed[1])
            values[1] = r[j++] * 4.0 / (1.0 + std::fabs(values[3])) * eps2();
        if (!paramIsFixed[0])
            values[0] = r[j++] * expiryTime -
                        eps2() * (values[1] * values[2] *
                                  std::sqrt(1.0 - values[3] * values[3]));
    }
    Array inverse(const Array &y, const std::vector<bool> &,
                  const std::vector<Real> &, const Real) {
        Array x(5);
        x[2] = std::sqrt(y[2] - eps1());
        x[3] = std::asin(y[3] / eps2());
        x[4] = y[4];
        x[1] = std::tan(y[1] / 4.0 * (1.0 + std::fabs(y[3])) / eps2() * M_PI -
                        M_PI / 2.0);
        x[0] = std::sqrt(y[0] - eps1() +
                         y[1] * y[2] * std::sqrt(1.0 - y[3] * y[3]));
        return x;
    }
    Real eps1() { return 0.000001; }
    Real eps2() { return 0.999999; }
    Array direct(const Array &x, const std::vector<bool> &paramIsFixed,
                 const std::vector<Real> &params, const Real forward) {
        Array y(5);
        y[2] = x[2] * x[2] + eps1();
        y[3] = std::sin(x[3]) * eps2();
        y[4] = x[4];
        if (paramIsFixed[1])
            y[1] = params[1];
        else
            y[1] = (std::atan(x[1]) + M_PI / 2.0) / M_PI * eps2() * 4.0 /
                   (1.0 + std::fabs(y[3]));
        if (paramIsFixed[0])
            y[0] = params[0];
        else
            y[0] = eps1() + x[0] * x[0] -
                   y[1] * y[2] * std::sqrt(1.0 - y[3] * y[3]);
        return y;
    }
    Real weight(const Real strike, const Real forward, const Real stdDev,
                const std::vector<Real> &addParams) {
        return blackFormulaStdDevDerivative(strike, forward, stdDev, 1.0);
    }
    typedef SviWrapper type;
    ext::shared_ptr<type> instance(const Time t, const Real &forward,
                                     const std::vector<Real> &params,
                                     const std::vector<Real> &addParams) {
        return ext::make_shared<type>(t, forward, params);
    }
};

struct NoArbSVISpecs: SviSpecs {

    void checkNoArb(std::vector<Real> &params) {
        QL_REQUIRE(std::find(params.begin(), params.end(), Null<Real>()) == params.end(),
                   "all parameters must be set to check for arbitrage");
        QL_REQUIRE(params[2] > 0.0, "sigma (" << params[2] << ") must be positive");
        Real aOverSigma = params[0] / params[2];
        Real mOverSigma = params[4] / params[2];
        Real minAOverSigma = svi::minAOverSigma(params[1], params[3]);
        QL_REQUIRE(aOverSigma > minAOverSigma || std::fabs(params[3]) >= 1.0 && aOverSigma >= 0.0,
                   "a/sigma (" << aOverSigma << "must be above" << minAOverSigma <<
                   "to prevent butterfly arb");
        auto [lower, upper] = svi::mOverSigmaRange(aOverSigma, params[1], params[3]);
        QL_REQUIRE(mOverSigma > lower && mOverSigma < upper,
                   "m/sigma (" << mOverSigma << ") must be in (" << lower << ","
                   << upper << ") to prevent butterfly arb");
        Real minSigma = svi::minSigma(aOverSigma, params[1], params[3], mOverSigma);
        QL_REQUIRE(params[2] >= minSigma, "sigma (" << params[2] << ") must be at least" <<
                   minSigma << "to prevent butterfly arb");
    }
    void defaultValues(std::vector<Real> &params,
                       std::vector<bool> &paramIsFixed,
                       const Real &forward,
                       const Real expiryTime,
                       const std::vector<Real> &addParams) {
        if  (std::all_of(paramIsFixed.begin(), paramIsFixed.end(), [](bool f) {return f;})) {
            checkNoArb(params);
            return;
        }
        QL_REQUIRE(!paramIsFixed[0] && !paramIsFixed[2] && !paramIsFixed[4],
                   "a, sigma, and m cannot be fixed in the no arb svi parameterization");
        if (paramIsFixed[1])
            QL_REQUIRE(params[1] > 0.0 && params[1] <= 2.0,
                       "b (" << params[1] << ") must be (0, 2]");
        if (paramIsFixed[3])
            QL_REQUIRE(std::fabs(params[3]) <= 1.0,
                       "rho (" << params[3] << "must be in [-1, 1]");
        if (params[3] == Null<Real>())
            params[3] = -0.4;  // from SviSpecs default
        if (params[1] == Null<Real>())
            params[1] = 1.0 / (1.0 + std::fabs(params[3]));
        SviSpecs::defaultValues(params, paramIsFixed, forward, expiryTime, addParams);
    }
    void guess(Array &values, const std::vector<bool> &paramIsFixed,
               const Real &forward, const Real expiryTime,
               const std::vector<Real> &r, const std::vector<Real> &addParams) {
        SviSpecs::guess(values, paramIsFixed, forward, expiryTime, r, addParams);
        if (!paramIsFixed[1])
            values[1] /= 2.0;
    }
    Real rhoMax(const std::vector<bool> &paramIsFixed, const std::vector<Real> &params) {
        return paramIsFixed[1] ? std::clamp(2.0 / params[1] - 1.0, 0.0, eps2()) : eps2();
    }
    Real boundedB(Real x, Real rho) {
        return (eps1() + (1.0 -eps1()) * 0.5 * (1.0 + std::sin(x))) * 2.0 / (1.0 + std::fabs(rho)); 
    }
    Real boundedMOverSigma(Real x, Real lower, Real upper) {
        if (std::isinf(upper))
            return lower + eps1() + x * x;
        if (std::isinf(lower))
            return upper - eps1() - x * x;
        return 0.5 * (upper + lower) + 0.5 * (upper - lower) * eps2() * std::sin(x);
    }
    Array inverse(const Array &y, const std::vector<bool> &paramIsFixed,
                  const std::vector<Real> &params, const Real forward) {
        Array x(5, 0.0);
        Real rm = rhoMax(paramIsFixed, params);
        Real rho = paramIsFixed[3] ? params[3] : std::clamp(y[3], -rm, rm);
        if (!paramIsFixed[3] && rm > 0.0)
            x[3] = std::asin(rho / rm);
        Real b = params[1];
        if (!paramIsFixed[1]) {
            Real bFrac = std::clamp(y[1] * (1.0 + std::fabs(rho)) / 2.0, eps1(), 1.0);
            x[1] = std::asin(std::clamp(2.0 * (bFrac - eps1()) / (1.0 - eps1()) - 1.0, -1.0, 1.0));
            b = boundedB(x[1], rho);
        }
        Real minAOverSigma = svi::minAOverSigma(b, rho);
        x[0] = std::sqrt(std::max(y[0] / y[2] - minAOverSigma - eps1(), 0.0));
        Real aOverSigma = minAOverSigma + eps1() + x[0] * x[0];
        auto [lower, upper] = svi::mOverSigmaRange(aOverSigma, b, rho);
        Real mOverSigma = y[4] / y[2];
        if (std::isinf(upper))
            x[4] = std::sqrt(std::max(mOverSigma - lower - eps1(), 0.0));
        else if (std::isinf(lower))
            x[4] = std::sqrt(std::max(upper - eps1() - mOverSigma, 0.0));
        else
            x[4] = std::asin(std::clamp((2.0 * mOverSigma - lower - upper) / (
                (upper - lower) * eps2()), -1.0, 1.0));
        x[2] = std::sqrt(std::max(y[2] - minSigmaRelMargin() * svi::minSigma(
            aOverSigma, b, rho, boundedMOverSigma(x[4], lower, upper)), 0.0));
        return x;
    }
    Real minSigmaRelMargin() {return 1.0 + 1.0e-8;}
    Array direct(const Array &x, const std::vector<bool> &paramIsFixed,
                 const std::vector<Real> &params, const Real forward) {
        Real rho = paramIsFixed[3] ? params[3] : std::sin(x[3]) * rhoMax(paramIsFixed, params);
        Real b = paramIsFixed[1] ? params[1] : boundedB(x[1], rho);
        Real aOverSigma = svi::minAOverSigma(b, rho) + eps1() + x[0] * x[0];
        auto [lower, upper] = svi::mOverSigmaRange(aOverSigma, b, rho);
        Real mOverSigma = boundedMOverSigma(x[4], lower, upper);
        Real sigma = minSigmaRelMargin() * svi::minSigma(aOverSigma, b, rho, mOverSigma) + x[2] * x[2];
        return {sigma * aOverSigma, b, sigma, rho, sigma * mOverSigma};
    }
};
}

//! %Svi smile interpolation between discrete volatility points.
class SviInterpolation : public Interpolation {
  private:
    template <class F>
    decltype(auto) coeffs(const F& f) const {
        if (arbitrageFree_)
            return f(dynamic_cast<const detail::XABRCoeffHolder<detail::NoArbSVISpecs>&>(*impl_));
        return f(dynamic_cast<const detail::XABRCoeffHolder<detail::SviSpecs>&>(*impl_));
    }
    bool arbitrageFree_;
  public:
    template <class I1, class I2>
    SviInterpolation(const I1 &xBegin, // x = strikes
                     const I1 &xEnd,
                     const I2 &yBegin, // y = volatilities
                     Time t,           // option expiry
                     const Real &forward, Real a, Real b, Real sigma, Real rho,
                     Real m, bool aIsFixed, bool bIsFixed, bool sigmaIsFixed,
                     bool rhoIsFixed, bool mIsFixed, bool vegaWeighted = true,
                     const ext::shared_ptr<EndCriteria> &endCriteria =
                         ext::shared_ptr<EndCriteria>(),
                     const ext::shared_ptr<OptimizationMethod> &optMethod =
                         ext::shared_ptr<OptimizationMethod>(),
                     const Real errorAccept = 0.0020,
                     const bool useMaxError = false,
                     const Size maxGuesses = 50,
                     const bool arbitrageFree = false)
        : arbitrageFree_(arbitrageFree) {
        auto makeImpl = [&](auto specs) {
            return ext::shared_ptr<Interpolation::Impl>(
                new detail::XABRInterpolationImpl<I1, I2, decltype(specs)>(
                    xBegin, xEnd, yBegin, t, forward,
                    {a, b, sigma, rho, m},
                    {aIsFixed, bIsFixed, sigmaIsFixed, rhoIsFixed, mIsFixed},
                    vegaWeighted, endCriteria, optMethod, errorAccept, useMaxError,
                    maxGuesses));
        };
        impl_ = arbitrageFree_ ? makeImpl(detail::NoArbSVISpecs()) : makeImpl(detail::SviSpecs());
    }
    const std::vector<Real>& params() const {
        return coeffs([](const auto& c) -> const std::vector<Real>& {return c.params_;});
    }
    Real expiry() const { return coeffs([](const auto& c) {return c.t_;}); }
    Real forward() const { return coeffs([](const auto& c) {return c.forward_;}); }
    Real a() const { return params()[0]; }
    Real b() const { return params()[1]; }
    Real sigma() const { return params()[2]; }
    Real rho() const { return params()[3]; }
    Real m() const { return params()[4]; }
    Real rmsError() const { return coeffs([](const auto& c) {return c.error_;}); }
    Real maxError() const { return coeffs([](const auto& c) {return c.maxError_;}); }
    const std::vector<Real> &interpolationWeights() const {
        return coeffs([](const auto& c) -> const std::vector<Real>& {return c.weights_;});
    }
    EndCriteria::Type endCriteria() {
        return coeffs([](const auto& c) { return c.XABREndCriteria_; });
    }
};

//! %Svi interpolation factory and traits
class Svi {
  public:
    Svi(Time t,
        Real forward,
        Real a,
        Real b,
        Real sigma,
        Real rho,
        Real m,
        bool aIsFixed,
        bool bIsFixed,
        bool sigmaIsFixed,
        bool rhoIsFixed,
        bool mIsFixed,
        bool vegaWeighted = false,
        ext::shared_ptr<EndCriteria> endCriteria = ext::shared_ptr<EndCriteria>(),
        ext::shared_ptr<OptimizationMethod> optMethod = ext::shared_ptr<OptimizationMethod>(),
        const Real errorAccept = 0.0020,
        const bool useMaxError = false,
        const Size maxGuesses = 50,
        const bool arbitrageFree = false)
    : t_(t), forward_(forward), a_(a), b_(b), sigma_(sigma), rho_(rho), m_(m), aIsFixed_(aIsFixed),
      bIsFixed_(bIsFixed), sigmaIsFixed_(sigmaIsFixed), rhoIsFixed_(rhoIsFixed),
      mIsFixed_(mIsFixed), vegaWeighted_(vegaWeighted), endCriteria_(std::move(endCriteria)),
      optMethod_(std::move(optMethod)), errorAccept_(errorAccept), useMaxError_(useMaxError),
      maxGuesses_(maxGuesses), arbitrageFree_(arbitrageFree) {}
    template <class I1, class I2>
    Interpolation interpolate(const I1 &xBegin, const I1 &xEnd,
                              const I2 &yBegin) const {
        return SviInterpolation(xBegin, xEnd, yBegin, t_, forward_, a_, b_,
                                 sigma_, rho_, m_, aIsFixed_, bIsFixed_,
                                 sigmaIsFixed_, rhoIsFixed_, mIsFixed_,
                                 vegaWeighted_, endCriteria_, optMethod_,
                                 errorAccept_, useMaxError_, maxGuesses_,
                                 arbitrageFree_);
    }
    static const bool global = true;

  private:
    Time t_;
    Real forward_;
    Real a_, b_, sigma_, rho_, m_;
    bool aIsFixed_, bIsFixed_, sigmaIsFixed_, rhoIsFixed_, mIsFixed_;
    bool vegaWeighted_;
    const ext::shared_ptr<EndCriteria> endCriteria_;
    const ext::shared_ptr<OptimizationMethod> optMethod_;
    const Real errorAccept_;
    const bool useMaxError_;
    const Size maxGuesses_;
    const bool arbitrageFree_;
};
}

#endif
