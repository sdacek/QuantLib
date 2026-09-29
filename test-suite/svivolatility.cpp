/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2022 Skandinaviska Enskilda Banken AB (publ)

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

#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <ql/experimental/volatility/sviinterpolation.hpp>
#include <ql/experimental/volatility/svismilesection.hpp>
#include <algorithm>

using namespace QuantLib;
using namespace boost::unit_test_framework;


namespace {

    Real densityScaledParams(const std::vector<Real> &scaledParams, Real l) {
        Real aOverSigma = scaledParams[0], b = scaledParams[1];
        Real rho = scaledParams[2], mOverSigma = scaledParams[3];
        Real sigma = scaledParams[4];
        Real s = std::hypot(l, 1.0);
        Real n = aOverSigma + b * (rho * l + s);
        Real n1 = b * (rho + l / s);
        Real n2 = b / (s * s * s);
        Real c = 1.0 - n1 * (l + mOverSigma) / (2.0 * n);
        return (c - n1 / 4.0) * (c + n1 / 4.0) + (
                    n2 - n1 * n1 / (2.0 * n)) / (2.0 * sigma);
    }

    Real minDensityScaledParams(const std::vector<Real> &scaledParams) {
        Real result = QL_MAX_REAL;
        for (Real direction : {-1.0, 1.0}) {
            for (Real e = -4.0; e <= 12.0; e +=1e-4) {
                result = std::min(result, densityScaledParams(scaledParams, direction * std::pow(10.0, e)));
            }
        }
        return result;
    }

    std::vector<Real> toScaledParams(const std::vector<Real> &params) {
        return {params[0] / params[2], params[1], params[3], params[4] / params[2], params[2]};
    }

    Real minDensity(const std::vector<Real> &params) {
        return minDensityScaledParams(toScaledParams(params));
    }

    std::vector<Real> strikeGrid(Real maxLogMoneyness) {
        std::vector<Real> strikes;
        for (Real k=-maxLogMoneyness; k<=maxLogMoneyness + 1e-12; k+=0.25)
            strikes.push_back(std::exp(k));
        return strikes;
    }

    std::vector<Real> sviVols(const std::vector<Real>& params, const std::vector<Real>& strikes) {
        SviSmileSection svi(1.0, 1.0, params);
        std::vector<Real> vols;
        for (Real strike: strikes)
            vols.push_back(svi.volatility(strike));
        return vols;
    }

    SviInterpolation sviInterp(const std::vector<Real>& strikes, const std::vector<Real>& vols,
                               const bool arbFree,
                               const std::vector<Real>& params = std::vector<Real>(5, Null<Real>()),
                               const std::vector<bool>& isFixed = std::vector<bool>(5, false)) {
        SviInterpolation svi(strikes.begin(), strikes.end(), vols.begin(), 1.0, 1.0, params[0], params[1],
                             params[2], params[3], params[4], isFixed[0], isFixed[1], isFixed[2], isFixed[3],
                             isFixed[4], false, ext::shared_ptr<EndCriteria>(),
                             ext::shared_ptr<OptimizationMethod>(), 0.0020, false, 5, arbFree);
        svi.update();
        return svi;
    }

    std::vector<Real> parameters(const SviInterpolation& svi) {
        return {svi.a(), svi.b(), svi.sigma(), svi.rho(), svi.m()};
    }

    Real rmsError(const std::vector<Real>& params,
                  const std::vector<Real>& strikes,
                  const std::vector<Real>& vols) {
        return sviInterp(strikes, vols, false, params, std::vector<bool>(5, true)).rmsError();
    }
}

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(SviVolatilityTests)

BOOST_AUTO_TEST_CASE(testSviSmileSection) {

    BOOST_TEST_MESSAGE("Testing SviSmileSection construction...");

    Date today = Settings::instance().evaluationDate();

    // Test time based constructor
    Time tte = 11.0 / 365;
    Real forward = 123.45;
    Real a = -0.0666;
    Real b = 0.229;
    Real sigma = 0.337;
    Real rho = 0.439;
    Real m = 0.193;
    std::vector<Real> sviParameters = {a, b, sigma, rho, m};
    // Compute the strike that yields x (log-moneyness) equal to m,
    // this simplifies the variance expression to a+b*sigma so we can test the correctness
    // against the input parameters
    Real strike = forward * std::exp(m);
    ext::shared_ptr<SviSmileSection> time_section;

    BOOST_CHECK_NO_THROW(time_section =
                             ext::make_shared<SviSmileSection>(tte, forward, sviParameters));
    BOOST_CHECK_EQUAL(time_section->atmLevel(), forward);
    QL_CHECK_CLOSE(time_section->variance(strike), a + b * sigma, 1E-10);

    // Test date based constructor
    Date date = today + Period(11, Days);
    ext::shared_ptr<SviSmileSection> date_section;

    BOOST_CHECK_NO_THROW(date_section =
                             ext::make_shared<SviSmileSection>(date, forward, sviParameters));

    BOOST_CHECK_EQUAL(date_section->atmLevel(), forward);
    QL_CHECK_CLOSE(date_section->variance(strike), a + b * sigma, 1E-10);
}

BOOST_AUTO_TEST_CASE(testNoArbSviDomain) {
    BOOST_TEST_MESSAGE("Testing svi no butterfly arbitrage domain...");
    // parameters pulled from "No arbitrage SVI" https://arxiv.org/pdf/2005.03340

    Real a = -0.041;
    Real b = 0.1331;
    Real rho = 0.306;
    Real m = 0.3586;
    Real sigma = 0.4153;
    BOOST_CHECK_CLOSE(detail::svi::minAOverSigma(b, rho), -0.12663, 1e-3);
    auto [lower, upper] = detail::svi::mOverSigmaRange(a / sigma, b, rho);
    BOOST_CHECK_CLOSE(lower, -0.72407, 1e-3);
    BOOST_CHECK_CLOSE(upper, 0.82939, 1e-3);
    BOOST_CHECK_GT(m / sigma, upper);
    for (Real bb : {0.5, 1.0 , 1.5}) {
        Real l = -6.0 * bb / std::sqrt(bb * bb * bb * bb - 20.0 * bb * bb + 64.0);
        Real s = std::sqrt(l * l + 1.0);
        Real expected = bb * (l * l / 4.0 * (2.0 * s + bb * l) - s);
        BOOST_CHECK_CLOSE(detail::svi::minAOverSigma(bb, 0.0), expected, 1e-8);
    }

    std::tie(lower, upper) = detail::svi::mOverSigmaRange(0.0, 0.5, -1.0);
    BOOST_CHECK_CLOSE(lower, -std::sqrt(1.5), 1e-8);
    BOOST_CHECK(std::isinf(upper));
    BOOST_CHECK_GT(upper, 0.0);

    BOOST_CHECK_EQUAL(detail::svi::minAOverSigma(2.0, 0.0), 0.0);
    std::tie(lower, upper) = detail::svi::mOverSigmaRange(0.3, 2.0, 0.0);
    BOOST_CHECK_CLOSE(lower, -0.15, 1e-12);
    BOOST_CHECK_CLOSE(upper, 0.15, 1e-12);

    a = -0.0198444;
    b = 0.102745;
    rho = 0.180754;
    m = 0.266125;
    sigma = 0.310459;

    auto withSigma = [&](Real s) {
        return std::vector<Real>{a / sigma * s, b, s, rho, m / sigma * s};
    };

    Real minSigma = detail::svi::minSigma(a / sigma, b, rho, m / sigma);
    BOOST_CHECK_GT(minDensity(withSigma(minSigma)), -1e-10);
    BOOST_CHECK_LT(minDensity(withSigma(minSigma * 0.999)), 0.0);
    for (Real direction : {-1.0, 1.0}) {
        Real bLimit = 1.4;
        Real rhoLimit = direction * (2.0 / bLimit - 1.0);
        Real aLimit = detail::svi::minAOverSigma(bLimit, rhoLimit) + 0.3;
        std::tie(lower, upper) = detail::svi::mOverSigmaRange(aLimit, bLimit, rhoLimit);
        Real mLimit = 0.5 * (lower + upper) + 0.25 * direction * (upper - lower);
        BOOST_CHECK_CLOSE(detail::svi::minSigma(aLimit, bLimit, rhoLimit, mLimit),
                                   2.0 / (aLimit - 2.0 * direction * mLimit), 1e-12);
    }
    for (Real r: {0.0, 0.5, 0.999, 1.0}) {
        std::tie(lower, upper) = detail::svi::mOverSigmaRange(0.1, 0.5, -r);
        Real mOverSigma = std::isinf(upper) ? lower + 0.5: 0.5 * (lower + upper);
        BOOST_CHECK_CLOSE(detail::svi::minSigma(0.1, 0.5, -r, mOverSigma),
                                   detail::svi::minSigma(0.1, 0.5, r, -mOverSigma), 1e-9);
    }
}

BOOST_AUTO_TEST_CASE(testNoArbSviFixedFreeParameters) {
    BOOST_TEST_MESSAGE("Testing fixed parameters in arbitrage free svi");

    std::vector<Real> params = {0.02, 0.1, 0.3, -0.3, 0.05};
    Time t= 1.0;
    Real forward = 1.0;
    auto strikes = strikeGrid(1.0);
    auto vols = sviVols(params, strikes);
    BOOST_CHECK_THROW(sviInterp(strikes, vols, true, params, {true, false, false, false, false}),
                      Error);

    auto fixedRho = sviInterp(strikes, vols, true, params, {false, false, false, true, false});
    BOOST_CHECK_EQUAL(fixedRho.rho(), params[3]);
    BOOST_CHECK_GE(minDensity(parameters(fixedRho)), 0.0);
    QL_CHECK_SMALL(fixedRho.rmsError(), 1e-10);

    auto fixedB = sviInterp(strikes, vols, true, params, {false, true, false, false, false});
    BOOST_CHECK_EQUAL(fixedB.b(), params[1]);
    BOOST_CHECK_GE(minDensity(parameters(fixedB)), 0.0);
    QL_CHECK_SMALL(fixedB.rmsError(), 1e-8);

    std::vector<bool> allFixed(5, true);
    std::vector<Real> noArbSmile = {-0.0305199, 0.102717, 0.412398, 0.100718, 0.272344};
    auto noArbVols = sviVols(noArbSmile, strikes);
    BOOST_CHECK(parameters(sviInterp(strikes, noArbVols, true, noArbSmile, allFixed)) == noArbSmile);
    std::vector<Real> arbSmile = {-0.041, 0.1331, 0.4153, 0.306, 0.3586};
    auto arbVols = sviVols(arbSmile, strikes);
    BOOST_CHECK_THROW(sviInterp(strikes, arbVols, true, arbSmile, allFixed), Error);
}

BOOST_AUTO_TEST_CASE(testNoArbSviArbFreeRecovery) {
    BOOST_TEST_MESSAGE("Testing no arb svi recoveres arb free smiles in the presence of arb");
    Time t = 1.0;
    Real forward = 1.0;
    auto strikes = strikeGrid(1.5);
    auto fit = [&](const std::vector<Real>& params){
        auto svi = sviInterp(strikes, sviVols(params, strikes), true);
        SviSmileSection section(1.0, 1.0, parameters(svi));
        for (Real strike: strikes) {
            BOOST_CHECK_CLOSE(svi(strike), section.volatility(strike), 1e-12);
        }
        BOOST_CHECK_LE(svi.b() * (1.0 + std::fabs(svi.rho())), 2.0);
        std::vector<Real> fit = {svi.a(), svi.b(), svi.sigma(), svi.rho(), svi.m()};
        BOOST_CHECK_GE(minDensity(fit), 0.0);
        return std::pair{fit, svi.maxError()};
    };

    std::vector<Real> arbSmile = {-0.041, 0.1331, 0.4153, 0.306, 0.3586};
    BOOST_CHECK_LT(minDensity(arbSmile), 0.0);
    auto arbVols = sviVols(arbSmile, strikes);
    auto raw = sviInterp(strikes, arbVols, false);
    QL_CHECK_SMALL(raw.maxError(), 1e-10);
    BOOST_CHECK_LT(minDensity(parameters(raw)), -1e-10);
    auto [arbFreeFit, arbFreeError] = fit(arbSmile);
    BOOST_CHECK_GT(arbFreeError, 1e-3);
    std::vector<Real> repairedSmile = {-0.0305199, 0.102717, 0.412398, 0.100718, 0.272344};
    BOOST_CHECK_GE(minDensity(repairedSmile), 0.0);
    BOOST_CHECK_LE(rmsError(arbFreeFit, strikes, arbVols), rmsError(repairedSmile, strikes, arbVols));
    BOOST_CHECK_SMALL(fit(repairedSmile).second, 1e-10);
    BOOST_CHECK_SMALL(fit(arbFreeFit).second, 1e-10);
}

BOOST_AUTO_TEST_CASE(testRawAndNoArbConverge) {
    BOOST_TEST_MESSAGE("Tesing no arb and raw SVI agree on smiles without arb");
    auto strikes = strikeGrid(1.5);
    auto arbVols = sviVols({-0.041, 0.1331, 0.4153, 0.306, 0.3586}, strikes);
    auto cases = {parameters(sviInterp(strikes, arbVols, true)),
                  {-0.0305199, 0.102717, 0.412398, 0.100718, 0.272344},
                  {0.02, 0.1, 0.3, -0.3, 0.05}};
    for (auto smile: cases) {
        auto vols = sviVols(smile, strikes);
        auto raw = parameters(sviInterp(strikes, vols, false));
        auto noArb = parameters(sviInterp(strikes, vols, true));
        for (Size i = 0; i < smile.size(); ++i) {
            BOOST_CHECK_SMALL(raw[i] - smile[i], 1e-10);
            BOOST_CHECK_SMALL(noArb[i] - smile[i], 1e-10);
        }
    }
}

BOOST_AUTO_TEST_CASE(testArbFreeCalibrationLimits) {
    BOOST_TEST_MESSAGE("Testing no arb svi minimum sigma and calibration at domain limits");

    std::vector<std::vector<Real>> cases(
        {std::vector<Real>{1.8155934301454117, 0.43570475056472885, 0.99999898730157977, -6494576.9914466767},
         std::vector<Real>{0.56779449170643015, 0.064023150903406961, 0.99999834544993094, -6869079.2569283862}}
    );
    for (const std::vector<Real> part : cases) {
        auto params = part;
        params.push_back(detail::svi::minSigma(part[0], part[1], part[2], part[3]));
        BOOST_CHECK_GT(minDensityScaledParams(params), -1e-10);
        params[4] *= (1 - 1e-6);
        BOOST_CHECK_LT(minDensityScaledParams(params), -1e-8);
    }
    Real b = 0.3;
    Real rho = -0.99999;
    Real aOverSigma = detail::svi::minAOverSigma(b, rho) + 2.0;
    auto [lower, upper] = detail::svi::mOverSigmaRange(aOverSigma, b, rho);
    Real mOverSigma = lower + 0.95 * (upper - lower);
    Real sigma = 0.95 * detail::svi::minSigma(aOverSigma, b, rho, mOverSigma);
    BOOST_CHECK_LT(minDensityScaledParams({aOverSigma, b, rho, mOverSigma, sigma}), 0.0);
    auto strikes = strikeGrid(1.5);
    auto vols = sviVols({aOverSigma * sigma, b, sigma, rho, mOverSigma * sigma}, strikes);
    auto params = toScaledParams(parameters(sviInterp(strikes, vols, true)));
    BOOST_CHECK_GT(minDensityScaledParams(params), -1e-10);
    auto minSigma = detail::NoArbSVISpecs().minSigmaRelMargin() * detail::svi::minSigma(
        params[0], params[1], params[2], params[3]);
    BOOST_CHECK_CLOSE(params[4], minSigma, 1e-10);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
