#ifndef ANALYSIS_UNIT_H
#define ANALYSIS_UNIT_H

#include <math.h>
#include <string>

namespace Analysis {
class Unit {
 private:
  const double pi = atan2(0e0, -1e0);
  const double atomicMass = 1.66053892173e-27;
  const double electronRestMass = 9.1093829140e-31;
  const double elementaryCharge = 1.60217656535e-19;
  const double reducedPlanckConstant = 1.05457172647e-34;
  const double coulombForceConstant = 8.9875517873681e9;
  const double fineStructureConstant = 7.297352566417e-3;
  const double bohrRadius = 5.291772109217e-11;
  const double hartreeEnergy = 4.35974465054e-18;
  const double auMomentum = reducedPlanckConstant/bohrRadius;
  const double gauss = 1.e-4;
  const double nano = 1.e-9;
  const double milli = 1.e-3;
  const double degree = pi / 180e0;

 public:
  Unit();
  ~Unit();

  const double readElectronRestMass(const double) const;
  const double readMass(const double) const;
  const double readCharge(const double) const;
  const double readTime(const double) const;
  const double readLength(const double) const;
  const double readElectricPotential(const double) const;
  const double readMagnaticField(const double) const;
  const double readDegree(const double) const;

  const double writeLength(const double) const;
  const double writeTime(const double) const;
  const double writeMomentum(const double) const;
  const double writeEnergy(const double) const;
  const double writeDegree(const double) const;
};
}

#endif
