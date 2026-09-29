/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
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

#include <ql/errors.hpp>
#include <ql/experimental/volatility/noarbsvi.hpp>
#include <boost/math/tools/minima.hpp>
#include <boost/math/tools/toms748_solve.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace QuantLib::detail::svi {
namespace {
    // slack on slope arbitrage boundaries
    constexpr Real slopeLimitTolerance = 1.0e-14;
    // max number of rootfind iterations
    constexpr std::uintmax_t maxIterations = 200;
    // relative start of the a/sigma threshold above -b sqrt(1-rho^2)
    constexpr Real positiveVarianceOffset = 1.0e-8;
    // search range in log|l| around the curvatureZero of vol curvature
    constexpr Real logSearchWidth = 60.0;
    // search limit in |l| on a slope wing. A closed form limit is used beyond
    constexpr Real slopeLimitCap = 1.0e6;

    inline Real diffSq(Real l, Real r) {
        return (l - r) * (l + r);
    }

    inline Real sqrtOneMRhoSq(Real rho) {
        return std::sqrt(diffSq(1.0, rho));
    }

    bool atSlopeLimit(Real b, Real rho) {
        return b * (1.0 - rho) >= 2.0 - slopeLimitTolerance;
    }

    Real vertex(Real rho) {
        return std::fabs(rho) < 1.0 ? -rho / sqrtOneMRhoSq(rho) : 0.0;
    }

    Real scaledVariance(Real l, Real aOverSigma, Real b, Real rho) {
        return aOverSigma + b * (rho * l + std::hypot(l, 1.0));
    }

    Real scaledVarianceSlope(Real l, Real b, Real rho) {
        return b * (rho + l / std::hypot(l, 1.0));
    }

    Real scaledVarianceCurvature(Real l, Real b) {
        return b / std::pow(l * l + 1.0 , 1.5);
    }

    template <class F>
    Real rootFrom(const F& f, Real start, Real direction) {
        auto g = [&](Real t) { return f(start + direction * t); };
        std::uintmax_t maxIter = maxIterations;
        auto [lo, hi] = boost::math::tools::bracket_and_solve_root(
            g, 1.0, 2.0, g(0.0) < 0.0, boost::math::tools::eps_tolerance<Real>(), maxIter);
        QL_REQUIRE(maxIter < maxIterations,
                   "root search did not converge in " << maxIterations << "iterations");
        return start + direction * 0.5 * (lo + hi);
    }

    template <class F>
    Real boundedMaximum(const F& f, Real xMin, Real xMax) {
        constexpr int bits = std::numeric_limits<Real>::digits / 2;
        auto [x, negPeak] = boost::math::tools::brent_find_minima(
            [&f](Real y) { return -f(y); }, xMin, xMax, bits);
        Real delta = 4.0 * std::ldexp(1.0, 1 - bits) * std::max(1.0, std::fabs(x));
        Real undershoot = std::max(
            0.0, -negPeak - 0.5 * (f(std::min(x + delta, xMax)) + f(std::max(x - delta, xMin))));
        return -negPeak + undershoot;
    }

    void checkSlopeLimits(Real b, Real rho) {
        QL_REQUIRE(b > 0.0, "b (" << b << ") must be positive");
        QL_REQUIRE(std::fabs(rho) <= 1.0, "rho (" << rho << ") must be in [-1, 1]");
        QL_REQUIRE(b * (1.0 + std::fabs(rho)) <= (2.0 + slopeLimitTolerance),
                   "b(1+|rho|) must be less than or equal to 2, (b=" << b << ", rho=" << rho << ")");
    }

    Real mOverSigmaLowerBound(Real aOverSigma, Real b, Real rho) {
        if (rho >= 1.0)
            return -std::numeric_limits<Real>::infinity();
        if (atSlopeLimit(b, rho))
            return -aOverSigma / 2.0;
        auto f = [=](Real l) {
            Real s = std::hypot(l, 1.0);
            Real t = rho * s + l;
            Real criticalAOverSigma = t * t * (s * (0.5 + b * rho / 4.0) + b * l / 4.0) - (rho * l + s);    
            return b * criticalAOverSigma - aOverSigma;
        };
        Real l = rootFrom(f, vertex(rho), -1.0);
        return 2.0 * scaledVariance(l, aOverSigma, b, rho) * (
            1.0 / scaledVarianceSlope(l, b, rho) + 0.25) - l;
    }
}  // namespace

Real minAOverSigma(Real b, Real rho) {
    checkSlopeLimits(b, rho);
    if (std::fabs(rho) >= 1.0 || (atSlopeLimit(b, rho) && atSlopeLimit(b, -rho)))
        return 0.0;
    Real posVarianceBound = -b * sqrtOneMRhoSq(rho);
    auto rangeWidth = [=](Real aOverSigma) {
        return -mOverSigmaLowerBound(aOverSigma, b, -rho) - mOverSigmaLowerBound(aOverSigma, b, rho);
    };
    Real searchStart = posVarianceBound * (1.0 - positiveVarianceOffset);
    if (rangeWidth(searchStart) >= 0.0)
        return posVarianceBound;

    std::uintmax_t maxIter = maxIterations;
    auto [lower, upper] = boost::math::tools::toms748_solve(
        rangeWidth, searchStart, 0.0, boost::math::tools::eps_tolerance<Real>(), maxIter);
    QL_REQUIRE(maxIter < maxIterations,
               "root search did not converge in " << maxIterations << "iterations");
    return 0.5 * (lower + upper);
}

std::pair<Real, Real> mOverSigmaRange(Real aOverSigma, Real b, Real rho) {
    checkSlopeLimits(b, rho);
    if (std::fabs(rho) < 1.0)
        QL_REQUIRE(aOverSigma + b * sqrtOneMRhoSq(rho) > 0.0,
                   "a/sigma + b*sqrt(1-rho^2) (a/sigma=" << aOverSigma <<
                   ", b=" << b << ", rho=" << rho << ") must be positive");
    else
        QL_REQUIRE(aOverSigma >= 0.0, "a/sigma (" << aOverSigma <<
                   ") must be non negative when |rho| = 1");
    return {mOverSigmaLowerBound(aOverSigma, b, rho), -mOverSigmaLowerBound(aOverSigma, b, -rho)};
}

Real minSigma(Real aOverSigma, Real b, Real rho, Real mOverSigma) {
    auto skewDensity = [=](Real l) {
        Real slope = scaledVarianceSlope(l, b, rho);
        Real c = 1.0 - slope * (l + mOverSigma) / (2.0 * scaledVariance(l, aOverSigma, b, rho));
        return diffSq(c, slope / 4.0);
    };

    auto curvatureDensity = [=](Real l) {
        Real slope = scaledVarianceSlope(l, b, rho);
        return scaledVarianceCurvature(l, b) - slope * slope / (
            2.0 * scaledVariance(l, aOverSigma, b, rho));
    };

    Real minPoint = vertex(rho);
    auto wingMinSigma = [&](Real direction) {
        if (direction * rho <= -1.0)
            return 0.0;
        Real start = direction < 0.0 ? std::min(minPoint, 0.0) : std::max(minPoint, 0.0);
        Real curvatureZero = rootFrom(curvatureDensity, start, direction);
        bool isAtSlopeLimit = atSlopeLimit(b, -direction * rho);
        Real logStart = std::log(std::fabs(curvatureZero));
        Real logEnd = isAtSlopeLimit ? std::log(slopeLimitCap * (
            1 + std::fabs(mOverSigma) + std::fabs(curvatureZero))) : logStart + logSearchWidth;
        Real result = boundedMaximum(
            [&](Real u) {
                Real l = direction * std::exp(u);
                return -curvatureDensity(l) / (2.0 * skewDensity(l));
            }, logStart, logEnd
        );
        return isAtSlopeLimit ? std::max(result, 2.0 / (aOverSigma - 2.0 * direction * mOverSigma)) : result;
    };
    return std::max(wingMinSigma(-1.0), wingMinSigma(1.0));
}
} // namespace QuantLib::detail::svi
