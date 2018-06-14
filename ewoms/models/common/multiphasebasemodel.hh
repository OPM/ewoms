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
 * \copydoc Ewoms::MultiPhaseBaseModel
 */
#ifndef EWOMS_MULTI_PHASE_BASE_MODEL_HH
#define EWOMS_MULTI_PHASE_BASE_MODEL_HH

#include <opm/material/densead/Math.hpp>

#include "multiphasebaseproperties.hh"
#include "multiphasebaseproblem.hh"
#include "multiphasebaseextensivequantities.hh"

#include <ewoms/models/common/flux.hh>
#include <ewoms/disc/vcfv/vcfvdiscretization.hh>

#include <opm/material/fluidmatrixinteractions/NullMaterial.hpp>
#include <opm/material/fluidmatrixinteractions/MaterialTraits.hpp>
#include <opm/material/thermal/NullThermalConductionLaw.hpp>
#include <opm/material/thermal/NullSolidEnergyLaw.hpp>
#include <opm/material/common/Unused.hpp>

namespace Ewoms {
template <class TypeTag>
class MultiPhaseBaseModel;
}

namespace Ewoms {
namespace Properties {
//! The generic type tag for problems using the immiscible multi-phase model
NEW_TYPE_TAG(MultiPhaseBaseModel, INHERITS_FROM(VtkMultiPhase, VtkTemperature));

//! Specify the splices of the MultiPhaseBaseModel type tag
SET_SPLICES(MultiPhaseBaseModel, SpatialDiscretizationSplice);

//! Set the default spatial discretization
//!
//! We use a vertex centered finite volume method by default
SET_TAG_PROP(MultiPhaseBaseModel, SpatialDiscretizationSplice, VcfvDiscretization);

//! set the number of equations to the number of phases
SET_INT_PROP(MultiPhaseBaseModel, NumEq, GET_PROP_TYPE(TypeTag, Indices)::numEq);
//! The number of phases is determined by the fluid system
SET_INT_PROP(MultiPhaseBaseModel, NumPhases, GET_PROP_TYPE(TypeTag, FluidSystem)::numPhases);
//! Number of chemical species in the system
SET_INT_PROP(MultiPhaseBaseModel, NumComponents, GET_PROP_TYPE(TypeTag, FluidSystem)::numComponents);

//! The type of the base base class for actual problems
SET_TYPE_PROP(MultiPhaseBaseModel, BaseProblem, Ewoms::MultiPhaseBaseProblem<TypeTag>);

//! By default, use the Darcy relation to determine the phase velocity
SET_TYPE_PROP(MultiPhaseBaseModel, FluxModule, Ewoms::DarcyFluxModule<TypeTag>);

/*!
 * \brief Set the material law to the null law by default.
 */
SET_PROP(MultiPhaseBaseModel, MaterialLaw)
{
private:
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
    typedef Opm::NullMaterialTraits<Scalar, FluidSystem::numPhases> Traits;

public:
    typedef Opm::NullMaterial<Traits> type;
};

/*!
 * \brief Set the property for the material parameters by extracting
 *        it from the material law.
 */
SET_TYPE_PROP(MultiPhaseBaseModel,
              MaterialLawParams,
              typename GET_PROP_TYPE(TypeTag, MaterialLaw)::Params);

//! set the energy storage law for the solid to the one which assumes zero heat capacity
//! by default
SET_TYPE_PROP(MultiPhaseBaseModel,
              SolidEnergyLaw,
              Opm::NullSolidEnergyLaw<typename GET_PROP_TYPE(TypeTag, Scalar)>);

//! extract the type of the parameter objects for the solid energy storage law from the
//! law itself
SET_TYPE_PROP(MultiPhaseBaseModel,
              SolidEnergyLawParams,
              typename GET_PROP_TYPE(TypeTag, SolidEnergyLaw)::Params);

//! set the thermal conduction law to a dummy one by default
SET_TYPE_PROP(MultiPhaseBaseModel,
              ThermalConductionLaw,
              Opm::NullThermalConductionLaw<typename GET_PROP_TYPE(TypeTag, Scalar)>);

//! extract the type of the parameter objects for the thermal conduction law from the law
//! itself
SET_TYPE_PROP(MultiPhaseBaseModel,
              ThermalConductionLawParams,
              typename GET_PROP_TYPE(TypeTag, ThermalConductionLaw)::Params);

//! disable gravity by default
SET_BOOL_PROP(MultiPhaseBaseModel, EnableGravity, false);

} // namespace Properties

/*!
 * \ingroup MultiPhaseBaseModel
 * \brief A base class for fully-implicit multi-phase porous-media flow models
 *        which assume multiple fluid phases.
 */
template <class TypeTag>
class MultiPhaseBaseModel : public GET_PROP_TYPE(TypeTag, Discretization)
{
    typedef typename GET_PROP_TYPE(TypeTag, Discretization) ParentType;
    typedef typename GET_PROP_TYPE(TypeTag, Model) Implementation;
    typedef typename GET_PROP_TYPE(TypeTag, Simulator) Simulator;
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, Indices) Indices;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
    typedef typename GET_PROP_TYPE(TypeTag, ElementContext) ElementContext;
    typedef typename GET_PROP_TYPE(TypeTag, EqVector) EqVector;
    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;

    typedef typename GridView::template Codim<0>::Iterator ElementIterator;
    typedef typename GridView::template Codim<0>::Entity Element;

    enum { numPhases = GET_PROP_VALUE(TypeTag, NumPhases) };
    enum { numComponents = FluidSystem::numComponents };

public:
    MultiPhaseBaseModel(Simulator& simulator)
        : ParentType(simulator)
    { }

    /*!
     * \brief Register all run-time parameters for the immiscible model.
     */
    static void registerParameters()
    {
        ParentType::registerParameters();

        // register runtime parameters of the VTK output modules
        Ewoms::VtkMultiPhaseModule<TypeTag>::registerParameters();
        Ewoms::VtkTemperatureModule<TypeTag>::registerParameters();
    }

    /*!
     * \copydoc FvBaseDiscretization::finishInit()
     */
    void finishInit()
    {
        ParentType::finishInit();

        // Reduce the Newton tolerance by the volume of smallest degree of freedom. (For
        // large grids the tolerance needs to be reduced because the total mass lost
        // would be too large.)
        Scalar minDofVolume = 1e100;
        for (unsigned globalDofIdx = 0; globalDofIdx < this->numGridDof(); ++ globalDofIdx)
            if (this->isLocalDof(globalDofIdx))
                minDofVolume = std::min(minDofVolume, this->dofTotalVolume(globalDofIdx));
        minDofVolume = this->gridView().comm().min(minDofVolume);

        Scalar newtonTolerance = EWOMS_GET_PARAM(TypeTag, Scalar, NewtonRawTolerance);
        newtonTolerance /= std::sqrt(minDofVolume);
        this->newtonMethod().setTolerance(newtonTolerance);
    }

    /*!
     * \brief Returns true iff a fluid phase is used by the model.
     *
     * \param phaseIdx The index of the fluid phase in question
     */
    bool phaseIsConsidered(unsigned phaseIdx OPM_UNUSED) const
    { return true; }

    /*!
     * \brief Compute the total storage inside one phase of all
     *        conservation quantities.
     *
     * \copydetails Doxygen::storageParam
     * \copydetails Doxygen::phaseIdxParam
     */
    void globalPhaseStorage(EqVector& storage, unsigned phaseIdx)
    {
        assert(0 <= phaseIdx && phaseIdx < numPhases);

        storage = 0;

        {
            unsigned threadId = 0;
            ElementContext elemCtx(this->simulator_);
            ElementIterator elemIt = this->simulator_.gridView().template begin<0>();
            const ElementIterator& elemEndIt = this->simulator_.gridView().template end<0>();
            EqVector tmp;

            for (; elemIt != elemEndIt; ++ elemIt) {
                const Element& elem = *elemIt;
                if (elem.partitionType() != Dune::InteriorEntity)
                    continue; // ignore ghost and overlap elements

                elemCtx.updateStencil(elem);
                elemCtx.updateIntensiveQuantities(/*timeIdx=*/0);

                const auto& stencil = elemCtx.stencil(/*timeIdx=*/0);

                for (unsigned dofIdx = 0; dofIdx < elemCtx.numDof(/*timeIdx=*/0); ++dofIdx) {
                    const auto& scv = stencil.subControlVolume(dofIdx);
                    const auto& intQuants = elemCtx.intensiveQuantities(dofIdx, /*timeIdx=*/0);

                    tmp = 0;
                    this->localResidual(threadId).addPhaseStorage(tmp,
                                                                  elemCtx,
                                                                  dofIdx,
                                                                  /*timeIdx=*/0,
                                                                  phaseIdx);
                    tmp *= scv.volume()*intQuants.extrusionFactor();

                    storage += tmp;
                }
            }
        }

        storage = this->gridView_.comm().sum(storage);
    }

    void registerOutputModules_()
    {
        ParentType::registerOutputModules_();

        // add the VTK output modules which make sense for all multi-phase models
        this->addOutputModule(new Ewoms::VtkMultiPhaseModule<TypeTag>(this->simulator_));
        this->addOutputModule(new Ewoms::VtkTemperatureModule<TypeTag>(this->simulator_));
    }

private:
    const Implementation& asImp_() const
    { return *static_cast<const Implementation *>(this); }
};
} // namespace Ewoms

#endif
