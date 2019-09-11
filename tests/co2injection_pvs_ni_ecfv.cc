// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 *
 * \brief Test for the non-isothermal primary variable switching VCVF
 *        discretization.
 */
#include "config.h"

#include <ewoms/common/start.hh>
#include <ewoms/models/pvs/pvsmodel.hh>
#include <opm/models/discretization/ecfv/ecfvdiscretization.hh>
#include "problems/co2injectionproblem.hh"

BEGIN_PROPERTIES

NEW_TYPE_TAG(Co2InjectionPvsNiEcfvProblem, INHERITS_FROM(PvsModel, Co2InjectionBaseProblem));
SET_TAG_PROP(Co2InjectionPvsNiEcfvProblem, SpatialDiscretizationSplice, EcfvDiscretization);

SET_BOOL_PROP(Co2InjectionPvsNiEcfvProblem, EnableEnergy, true);

//! Use automatic differentiation to linearize the system of PDEs
SET_TAG_PROP(Co2InjectionPvsNiEcfvProblem, LocalLinearizerSplice, AutoDiffLocalLinearizer);

END_PROPERTIES

int main(int argc, char **argv)
{
    typedef TTAG(Co2InjectionPvsNiEcfvProblem) EcfvProblemTypeTag;
    return Opm::start<EcfvProblemTypeTag>(argc, argv);
}
