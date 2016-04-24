//
// Created by Daehyun You on 12/30/15.
//

#include <ctime>
#include <TChain.h>
#include <zconf.h>
#include "RUN.h"
Analysis::Run::Run(const std::string filename) {

  // Setup json file reader.
  Analysis::JSONReader configReader(filename);

  // Change the working directory.
  chdir(configReader.getStringAt("working_directory").c_str());

  // Setup writer.
  pLogWriter = new Analysis::LogWriter(configReader);
  pLogWriter->logResultOfLoadingJSONFile(configReader);

  // Setup ROOT files.
  if(configReader.getStringAt("setup_input.type") == "ROOT") {
    pChain = new TChain(configReader.getStringAt("setup_input.tree_name").c_str());
    pChain->Add(configReader.getStringAt("setup_input.filenames").c_str());
    pLogWriter->write() << "Input file type is ROOT. " << std::endl;
    pLogWriter->write() << "    Filenames: " << configReader.getStringAt("setup_input.filenames").c_str() << std::endl;
    pLogWriter->write() << "    Tree Name: " << configReader.getStringAt("setup_input.tree_name").c_str() << std::endl;
    pLogWriter->write() << std::endl;

    // Setup event reader.
    numberOfHits = configReader.getIntAt("setup_input.number_of_hits");
    pEventReader = new Analysis::EventDataReader(numberOfHits);
    char ch[2];
    for(int i=0; i<numberOfHits; i++) {
      for(std::string str : {"IonX", "IonY", "IonT", "ElecX", "ElecY", "ElecT"}) {
        sprintf(ch, "%01d", i);
        pChain->SetBranchAddress((str+ch).c_str(), &(pEventReader->setEventDataAt(i, str+ch)));
      }
      for(std::string str : {"IonFlag", "ElecFlag"}) {
        sprintf(ch, "%01d", i);
        pChain->SetBranchAddress((str+ch).c_str(), &(pEventReader->setFlagDataAt(i, str+ch)));
      }
    }
  }

  // Setup unit helper.
  pUnit = new Analysis::Unit;
  auto &unit = *pUnit;

  // Make analysis tools, ions, and electrons
  pTools = new Analysis::AnalysisTools(unit, configReader);
  auto &tools = *pTools;
  pIons = new Analysis::Ions(unit, configReader, numberOfHits);
  auto &ions = *pIons;
  pElectrons = new Analysis::Electrons(unit, configReader, numberOfHits);
  auto &electrons = *pElectrons;
  pLogWriter->logAnalysisTools(unit, tools, ions, electrons);

  // output option
  optionOfSendingOutOfFrame =
      configReader.getBoolAt("setup_output.send_out_of_frame");
  optionOfShowingOnlyMasterRegionEvents =
      configReader.getBoolAt("setup_output.show_only_master_region_events");
  pLogWriter->write() << "Output Options: " << std::endl;
  pLogWriter->write() << "    Send Out of Frame: "
      << (optionOfSendingOutOfFrame ? "true" : "false") << std::endl;
  pLogWriter->write() << "    Show Only Master Region Events: "
      << (optionOfSendingOutOfFrame ? "true" : "false") << std::endl;
  pLogWriter->write() << std::endl;

  // root
  {
    std::string str;
    str = tools.getID();
    if (str != "") {
      str += "-";
    }
    str += pLogWriter->getID();
    str += ".root";
    rootFile.Open(str.c_str(), "new");
  }

  // initialization is done
  pLogWriter->write() << "Initialization is done." << std::endl;
  pLogWriter->write() << std::endl;
}
Analysis::Run::~Run() {
  // counter
  pLogWriter->write() << "Event count: " << pTools->getEventNumber() << std::endl;

  // root
  writeFlags();
  writeIonBasicData();
  writeIonMomentumData();
  writeElectronBasicData();
  writeElectronMomentumData();
  writeIonAndElectronMomentumData();
  rootFile.Close();

  // finalization is done
  delete pElectrons;
  delete pIons;
  delete pTools;
  delete pUnit;
  delete pEventReader;
  delete pChain;

  pLogWriter->write() << "Finalization is done." << std::endl;
  pLogWriter->write() << std::endl;
  delete pLogWriter;
}
void Analysis::Run::processEvent(const size_t rawOfEntries) {
  // setup
  pChain->GetEntry((Long64_t) rawOfEntries);
  auto &reader = *pEventReader;
  auto &unit = *pUnit;
  auto &tools = *pTools;
  auto &ions = *pIons;
  auto &electrons = *pElectrons;

  // count event
  tools.loadEventCounter();

  // make sure ion and electron data is empty, and reset flags
  ions.resetEventData();
  electrons.resetEventData();
  int ionFlag = 0;
  int electronFlag = 0;

  // input event data
  tools.loadEventDataInputer(ions, unit, reader);
  tools.loadEventDataInputer(electrons, unit, reader);

  if (optionOfSendingOutOfFrame) {
    ions.setAllOfRealOrDummyObjectIsInFrameOfAllDataFlag();
    electrons.setAllOfRealOrDummyObjectIsInFrameOfAllDataFlag();
  }
  if (optionOfSendingOutOfFrame) {
    // dead data, don't plot basic data(x, y, TOF)
    { // ion
      const int &m = ions.getNumberOfRealOrDummyObjects();
      for (int i = 0; i < m; i++) {
        if (ions.getRealOrDummyIon(i).isDead()) {
          ions.setRealOrDummyObjectMembers(i).setFlagMembers().setOutOfFrameOfBasicDataFlag();
        }
      }
    }
    { // electron
      const int &m = electrons.getNumberOfRealOrDummyObjects();
      for (int i = 0; i < m; i++) {
        if (electrons.getRealOrDummyObject(i).isDead()) {
          electrons.setRealOrDummyObjectMembers(i).setFlagMembers().setOutOfFrameOfBasicDataFlag();
        }
      }
    }
    // dummy data, don't plot momentum
    // ion
    ions.setAllOfDummyObjectIsOutOfFrameOfMomentumDataFlag();
    // electron
    electrons.setAllOfDummyObjectIsOutOfFrameOfMomentumDataFlag();
  }

  // if all ion event data or all electron event data is dead, ignore the event
  const bool ionsAreAllDead = ions.areAllDeadRealAndDummyObjects();
  const bool electronsAreAllDead = electrons.areAllDeadRealAndDummyObjects();
  if (ionsAreAllDead || electronsAreAllDead) {
    // don't plot basic or momentum data
    if (optionOfSendingOutOfFrame) {
      ions.setAllOfRealOrDummyObjectIsOutOfFrameOfBasicDataFlag();
      electrons.setAllOfRealOrDummyObjectIsOutOfFrameOfBasicDataFlag();
    }
    if (ionsAreAllDead) {
      ionFlag = -11;
    } else {
      ionFlag = -12;
    }
    if (electronsAreAllDead) {
      electronFlag = -11;
    } else {
      electronFlag = -12;
    }
  } else { // calculate ion or electron momentum
    // ion
    // if all data is not within master region, don't calculate momentum
    const bool ionsAreAllWithinMasterRegion = ions.areAllWithinMasterRegion();
    if (!ionsAreAllWithinMasterRegion) {
      // don't plot momentum data
      if (optionOfSendingOutOfFrame) {
        ions.setAllOfObjectIsOutOfFrameOfMomentumDataFlag();
      }
      ionFlag = -20;
    } else { // calculate ion momentum
      tools.loadMomentumCalculator(ions);
      const bool ionsExistDeadObject = ions.existDeadObject();
      // if it couldn't calculate momentum, don't plot it
      if (ionsExistDeadObject) {
        if (optionOfSendingOutOfFrame) {
          ions.setAllOfObjectIsOutOfFrameOfMomentumDataFlag();
        }
        ionFlag = 1;
      } else {
        // succeed
        ionFlag = 10;
      }
    }

    // electron
    // if all data is not within master region, don't calculate momentum
    const bool electronsAreAllWithinMasterRegion =
        electrons.areAllWithinMasterRegion();
    if (!electronsAreAllWithinMasterRegion) {
      // don't plot momentum data
      if (optionOfSendingOutOfFrame) {
        electrons.setAllOfObjectIsOutOfFrameOfMomentumDataFlag();
      }
      electronFlag = -20;
    } else { // calculate electron momentum
      tools.loadMomentumCalculator(electrons);
      const bool electronsExistDeadObject = electrons.existDeadObject();
      // if it couldn't calculate momentum, don't plot it
      if (electronsExistDeadObject) {
        electronFlag = 1;
      } else {
        // succeed
        electronFlag = 10;
      }
    }
  }

  // histograms
  if (optionOfShowingOnlyMasterRegionEvents) {
	fillFlags();
	fillIonBasicData();
	if (ionFlag > 0 && electronFlag > 0) { fillIonMomentumData(); }
    fillElectronBasicData();
	if (ionFlag > 0 && electronFlag > 0) { fillElectronMomentumData(); }
    if (ionFlag > 0 && electronFlag > 0) { fillIonAndElectronMomentumData(); }
  } else {
	fillFlags();
	fillIonBasicData();
	fillIonMomentumData(); 
    fillElectronBasicData();
	fillElectronMomentumData();
    fillIonAndElectronMomentumData();
  }
}
const Analysis::Unit &Analysis::Run::getUnit() const {
  return *pUnit;
}
const Analysis::Ions &Analysis::Run::getIons() const {
  return *pIons;
}
const Analysis::Electrons &Analysis::Run::getElectrons() const {
  return *pElectrons;
}
const int& Analysis::Run::getNumberOfHitsUsed() const {
  return numberOfHits;
}
void Analysis::Run::fillIonBasicData() {
  const double &i1HitX = pIons->getRealOrDummyObject(0).getLocationX(*pUnit);
  const double &i1HitY = pIons->getRealOrDummyObject(0).getLocationY(*pUnit);
  const double &i2HitX = pIons->getRealOrDummyObject(1).getLocationX(*pUnit);
  const double &i2HitY = pIons->getRealOrDummyObject(1).getLocationY(*pUnit);
  const double &i3HitX = pIons->getRealOrDummyObject(2).getLocationX(*pUnit);
  const double &i3HitY = pIons->getRealOrDummyObject(2).getLocationY(*pUnit);
  const double &i4HitX = pIons->getRealOrDummyObject(3).getLocationX(*pUnit);
  const double &i4HitY = pIons->getRealOrDummyObject(3).getLocationY(*pUnit);
  const double &iCOMX = pIons->getLocationX(*pUnit);
  const double &iCOMY = pIons->getLocationY(*pUnit);
  const double &i1HitTOF = pIons->getRealOrDummyObject(0).getTOF(*pUnit);
  const double &i2HitTOF = pIons->getRealOrDummyObject(1).getTOF(*pUnit);
  const double &i3HitTOF = pIons->getRealOrDummyObject(2).getTOF(*pUnit);
  const double &i4HitTOF = pIons->getRealOrDummyObject(3).getTOF(*pUnit);
  const double &i1HitPlus2HitTOF = pIons->getSumOfTOF(*pUnit, 0, 1);
  const double &i2HitPlus3HitTOF = pIons->getSumOfTOF(*pUnit, 1, 2);
  root2DHistogramOf1stHitIonLocationX_LocationY.Fill(i1HitX, i1HitY);
  root2DHistogramOf2ndHitIonLocationX_LocationY.Fill(i2HitX, i2HitY);
  root2DHistogramOf3rdHitIonLocationX_LocationY.Fill(i3HitX, i3HitY);
  root2DHistogramOf4thHitIonLocationX_LocationY.Fill(i4HitX, i4HitY);
  root2DHistogramOfIonsCOMLocationX_LocationY.Fill(iCOMX, iCOMY);
  root1DHistogramOf1stHitIonTOF.Fill(i1HitTOF);
  root1DHistogramOf2ndHitIonTOF.Fill(i2HitTOF);
  root1DHistogramOf3rdHitIonTOF.Fill(i3HitTOF);
  root1DHistogramOf4thHitIonTOF.Fill(i4HitTOF);
  root2DHistogramOf1stHitIonTOF_2ndHitIonTOF.Fill(i1HitTOF, i2HitTOF);
  root2DHistogramOf2ndHitIonTOF_3rdHitIonTOF.Fill(i2HitTOF, i3HitTOF);
  if(pIons->getRealOrDummyIon(0).isWithinMasterRegion()) {
    root2DHistogramOf2ndHitIonTOF_3rdHitIonTOF_under1stHitIonMaster.Fill(i2HitTOF, i3HitTOF);
  }
  root2DHistogramOf3rdHitIonTOF_4thHitIonTOF.Fill(i3HitTOF, i4HitTOF);
  root2DHistogramOf1stHitIonTOFPlus2ndHitIonTOF_3rdHitIonTOF.Fill(
      i1HitPlus2HitTOF,
      i3HitTOF);
  root2DHistogramOf2ndHitIonTOFPlus3rdHitIonTOF_4thHitIonTOF.Fill(
      i2HitPlus3HitTOF,
      i4HitTOF);
}
void Analysis::Run::fillFlags() {

}
void Analysis::Run::fillIonMomentumData() {
  const double &i1HitPx = pIons->getRealOrDummyObject(0).getMomentumX(*pUnit);
  const double &i1HitPy = pIons->getRealOrDummyObject(0).getMomentumY(*pUnit);
  const double &i1HitPz = pIons->getRealOrDummyObject(0).getMomentumZ(*pUnit);
  const double &i1HitE = pIons->getRealOrDummyObject(0).getEnergy(*pUnit);
  const double &i2HitPx = pIons->getRealOrDummyObject(1).getMomentumX(*pUnit);
  const double &i2HitPy = pIons->getRealOrDummyObject(1).getMomentumY(*pUnit);
  const double &i2HitPz = pIons->getRealOrDummyObject(1).getMomentumZ(*pUnit);
  const double &i2HitE = pIons->getRealOrDummyObject(1).getEnergy(*pUnit);
  const double &i3HitPx = pIons->getRealOrDummyObject(2).getMomentumX(*pUnit);
  const double &i3HitPy = pIons->getRealOrDummyObject(2).getMomentumY(*pUnit);
  const double &i3HitPz = pIons->getRealOrDummyObject(2).getMomentumZ(*pUnit);
  const double &i3HitE = pIons->getRealOrDummyObject(2).getEnergy(*pUnit);
  const double &i4HitPx = pIons->getRealOrDummyObject(3).getMomentumX(*pUnit);
  const double &i4HitPy = pIons->getRealOrDummyObject(3).getMomentumY(*pUnit);
  const double &i4HitPz = pIons->getRealOrDummyObject(3).getMomentumZ(*pUnit);
  const double &i4HitE = pIons->getRealOrDummyObject(3).getEnergy(*pUnit);
  const double &iTotalE = pIons->getEnergy(*pUnit);
  root2DHistogramOf1stHitIonMomentumXY.Fill(i1HitPx, i1HitPy);
  root2DHistogramOf2ndHitIonMomentumXY.Fill(i2HitPx, i2HitPy);
  root2DHistogramOf3rdHitIonMomentumXY.Fill(i3HitPx, i3HitPy);
  root2DHistogramOf4thHitIonMomentumXY.Fill(i4HitPx, i4HitPy);
  root1DHistogramOf1stHitIonMomentumZ.Fill(i1HitPz);
  root1DHistogramOf2ndHitIonMomentumZ.Fill(i2HitPz);
  root1DHistogramOf3rdHitIonMomentumZ.Fill(i3HitPz);
  root1DHistogramOf4thHitIonMomentumZ.Fill(i4HitPz);
  root1DHistogramOf1stHitIonEnergy.Fill(i1HitE);
  root1DHistogramOf2ndHitIonEnergy.Fill(i2HitE);
  root1DHistogramOf3rdHitIonEnergy.Fill(i3HitE);
  root1DHistogramOf4thHitIonEnergy.Fill(i4HitE);
  root1DHistogramOfIonsTotalEnergy.Fill(iTotalE);
}
void Analysis::Run::fillElectronBasicData() {
  const double
      &e1HitX = pElectrons->getRealOrDummyObject(0).getLocationX(*pUnit);
  const double
      &e1HitY = pElectrons->getRealOrDummyObject(0).getLocationY(*pUnit);
  const double
      &e2HitX = pElectrons->getRealOrDummyObject(1).getLocationX(*pUnit);
  const double
      &e2HitY = pElectrons->getRealOrDummyObject(1).getLocationY(*pUnit);
  const double
      &e3HitX = pElectrons->getRealOrDummyObject(2).getLocationX(*pUnit);
  const double
      &e3HitY = pElectrons->getRealOrDummyObject(2).getLocationY(*pUnit);
  const double
      &e4HitX = pElectrons->getRealOrDummyObject(3).getLocationX(*pUnit);
  const double
      &e4HitY = pElectrons->getRealOrDummyObject(3).getLocationY(*pUnit);
  const double &eCOMX = pElectrons->getLocationX(*pUnit);
  const double &eCOMY = pElectrons->getLocationY(*pUnit);
  const double &eCOMThetaXY = pElectrons->getLocationalDirectionXY(*pUnit);
  const double &eCOMR = pElectrons->getLocation(*pUnit);
  const double &e1HitTOF = pElectrons->getRealOrDummyObject(0).getTOF(*pUnit);
  const double &e2HitTOF = pElectrons->getRealOrDummyObject(1).getTOF(*pUnit);
  const double &e3HitTOF = pElectrons->getRealOrDummyObject(2).getTOF(*pUnit);
  const double &e4HitTOF = pElectrons->getRealOrDummyObject(3).getTOF(*pUnit);
  root2DHistogramOf1stHitElectronLocationX_LocationY.Fill(e1HitX, e1HitY);
  root2DHistogramOf2ndHitElectronLocationX_LocationY.Fill(e2HitX, e2HitY);
  root2DHistogramOf3rdHitElectronLocationX_LocationY.Fill(e3HitX, e3HitY);
  root2DHistogramOf4thHitElectronLocationX_LocationY.Fill(e4HitX, e4HitY);
  root2DHistogramOfElectronsCOMLocationX_LocationY.Fill(eCOMX, eCOMY);
  root2DHistogramOfElectronsCOMLocationalDirectionXY_Radius.Fill(eCOMThetaXY,
                                                                 eCOMR);
  root1DHistogramOf1stHitElectronTOF.Fill(e1HitTOF);
  root1DHistogramOf2ndHitElectronTOF.Fill(e2HitTOF);
  root1DHistogramOf3rdHitElectronTOF.Fill(e3HitTOF);
  root1DHistogramOf4thHitElectronTOF.Fill(e4HitTOF);
}
void Analysis::Run::fillElectronMomentumData() {
  const double
      &e1HitPx = pElectrons->getRealOrDummyObject(0).getMomentumX(*pUnit);
  const double
      &e1HitPy = pElectrons->getRealOrDummyObject(0).getMomentumY(*pUnit);
  const double
      &e1HitPz = pElectrons->getRealOrDummyObject(0).getMomentumZ(*pUnit);
  const double &e1HitE = pElectrons->getRealOrDummyObject(0).getEnergy(*pUnit);
  const double
      &e2HitPx = pElectrons->getRealOrDummyObject(1).getMomentumX(*pUnit);
  const double
      &e2HitPy = pElectrons->getRealOrDummyObject(1).getMomentumY(*pUnit);
  const double
      &e2HitPz = pElectrons->getRealOrDummyObject(1).getMomentumZ(*pUnit);
  const double &e2HitE = pElectrons->getRealOrDummyObject(1).getEnergy(*pUnit);
  const double
      &e3HitPx = pElectrons->getRealOrDummyObject(2).getMomentumX(*pUnit);
  const double
      &e3HitPy = pElectrons->getRealOrDummyObject(2).getMomentumY(*pUnit);
  const double
      &e3HitPz = pElectrons->getRealOrDummyObject(2).getMomentumZ(*pUnit);
  const double &e3HitE = pElectrons->getRealOrDummyObject(2).getEnergy(*pUnit);
  const double
      &e4HitPx = pElectrons->getRealOrDummyObject(3).getMomentumX(*pUnit);
  const double
      &e4HitPy = pElectrons->getRealOrDummyObject(3).getMomentumY(*pUnit);
  const double
      &e4HitPz = pElectrons->getRealOrDummyObject(3).getMomentumZ(*pUnit);
  const double &e4HitE = pElectrons->getRealOrDummyObject(3).getEnergy(*pUnit);
  const double &eTotalP = pElectrons->getMomentum(*pUnit);
  const double &eTotalThetaXY = pElectrons->getMotionalDirectionXY(*pUnit);
  const double &eTotalThetaZX = pElectrons->getMotionalDirectionZX(*pUnit);
  const double &eTotalThetaZY = pElectrons->getMotionalDirectionZY(*pUnit);
  const double &eTotalE = pElectrons->getEnergy(*pUnit);
  root2DHistogramOf1stHitElectronMomentumXY.Fill(e1HitPx, e1HitPy);
  root2DHistogramOf2ndHitElectronMomentumXY.Fill(e2HitPx, e2HitPy);
  root2DHistogramOf3rdHitElectronMomentumXY.Fill(e3HitPx, e3HitPy);
  root2DHistogramOf4thHitElectronMomentumXY.Fill(e4HitPx, e4HitPy);
  root1DHistogramOf1stHitElectronMomentumZ.Fill(e1HitPz);
  root1DHistogramOf2ndHitElectronMomentumZ.Fill(e2HitPz);
  root1DHistogramOf3rdHitElectronMomentumZ.Fill(e3HitPz);
  root1DHistogramOf4thHitElectronMomentumZ.Fill(e4HitPz);
  root2DHistogramOfElectronsTotalMotionalDirectionXY_Momentum.Fill(eTotalThetaXY,
                                                                   eTotalP);
  root2DHistogramOfElectronsTotalMotionalDirectionZX_Momentum.Fill(eTotalThetaZX,
                                                                   eTotalP);
  root2DHistogramOfElectronsTotalMotionalDirectionZY_Momentum.Fill(eTotalThetaZY,
                                                                   eTotalP);
  root1DHistogramOf1stHitElectronEnergy.Fill(e1HitE);
  root1DHistogramOf2ndHitElectronEnergy.Fill(e2HitE);
  root1DHistogramOf3rdHitElectronEnergy.Fill(e3HitE);
  root1DHistogramOf4thHitElectronEnergy.Fill(e4HitE);
  root2DHistogramOf1stHitElectronEnergy_2ndHitElectronEnergy.Fill(e1HitE,
                                                                  e2HitE);
  root2DHistogramOf2ndHitElectronEnergy_3rdHitElectronEnergy.Fill(e2HitE,
                                                                  e3HitE);
  root2DHistogramOf3rdHitElectronEnergy_4thHitElectronEnergy.Fill(e3HitE,
                                                                  e4HitE);
  root1DHistogramOfElectronsTotalEnergy.Fill(eTotalE);
}
void Analysis::Run::writeFlags() {

}
void Analysis::Run::writeIonBasicData() {
  root2DHistogramOf1stHitIonLocationX_LocationY.Write();
  root2DHistogramOf2ndHitIonLocationX_LocationY.Write();
  root2DHistogramOf3rdHitIonLocationX_LocationY.Write();
  root2DHistogramOf4thHitIonLocationX_LocationY.Write();
  root2DHistogramOfIonsCOMLocationX_LocationY.Write();
  root1DHistogramOf1stHitIonTOF.Write();
  root1DHistogramOf2ndHitIonTOF.Write();
  root1DHistogramOf3rdHitIonTOF.Write();
  root1DHistogramOf4thHitIonTOF.Write();
  root2DHistogramOf1stHitIonTOF_2ndHitIonTOF.Write();
  root2DHistogramOf2ndHitIonTOF_3rdHitIonTOF.Write();
  root2DHistogramOf2ndHitIonTOF_3rdHitIonTOF_under1stHitIonMaster.Write();
  root2DHistogramOf3rdHitIonTOF_4thHitIonTOF.Write();
  root2DHistogramOf1stHitIonTOFPlus2ndHitIonTOF_3rdHitIonTOF.Write();
  root2DHistogramOf2ndHitIonTOFPlus3rdHitIonTOF_4thHitIonTOF.Write();
}
void Analysis::Run::writeIonMomentumData() {
  root2DHistogramOf1stHitIonMomentumXY.Write();
  root2DHistogramOf2ndHitIonMomentumXY.Write();
  root2DHistogramOf3rdHitIonMomentumXY.Write();
  root2DHistogramOf4thHitIonMomentumXY.Write();
  root1DHistogramOf1stHitIonMomentumZ.Write();
  root1DHistogramOf2ndHitIonMomentumZ.Write();
  root1DHistogramOf3rdHitIonMomentumZ.Write();
  root1DHistogramOf4thHitIonMomentumZ.Write();
  root1DHistogramOf1stHitIonEnergy.Write();
  root1DHistogramOf2ndHitIonEnergy.Write();
  root1DHistogramOf3rdHitIonEnergy.Write();
  root1DHistogramOf4thHitIonEnergy.Write();
  root1DHistogramOfIonsTotalEnergy.Write();
}
void Analysis::Run::writeElectronBasicData() {
  root2DHistogramOf1stHitElectronLocationX_LocationY.Write();
  root2DHistogramOf2ndHitElectronLocationX_LocationY.Write();
  root2DHistogramOf3rdHitElectronLocationX_LocationY.Write();
  root2DHistogramOf4thHitElectronLocationX_LocationY.Write();
  root2DHistogramOfElectronsCOMLocationX_LocationY.Write();
  root2DHistogramOfElectronsCOMLocationalDirectionXY_Radius.Write();
  root1DHistogramOf1stHitElectronTOF.Write();
  root1DHistogramOf2ndHitElectronTOF.Write();
  root1DHistogramOf3rdHitElectronTOF.Write();
  root1DHistogramOf4thHitElectronTOF.Write();
}
void Analysis::Run::writeElectronMomentumData() {
  root2DHistogramOf1stHitElectronMomentumXY.Write();
  root2DHistogramOf2ndHitElectronMomentumXY.Write();
  root2DHistogramOf3rdHitElectronMomentumXY.Write();
  root2DHistogramOf4thHitElectronMomentumXY.Write();
  root1DHistogramOf1stHitElectronMomentumZ.Write();
  root1DHistogramOf2ndHitElectronMomentumZ.Write();
  root1DHistogramOf3rdHitElectronMomentumZ.Write();
  root1DHistogramOf4thHitElectronMomentumZ.Write();
  root2DHistogramOfElectronsTotalMotionalDirectionXY_Momentum.Write();
  root2DHistogramOfElectronsTotalMotionalDirectionZX_Momentum.Write();
  root2DHistogramOfElectronsTotalMotionalDirectionZY_Momentum.Write();
  root1DHistogramOf1stHitElectronEnergy.Write();
  root1DHistogramOf2ndHitElectronEnergy.Write();
  root1DHistogramOf3rdHitElectronEnergy.Write();
  root1DHistogramOf4thHitElectronEnergy.Write();
  root2DHistogramOf1stHitElectronEnergy_2ndHitElectronEnergy.Write();
  root2DHistogramOf2ndHitElectronEnergy_3rdHitElectronEnergy.Write();
  root2DHistogramOf3rdHitElectronEnergy_4thHitElectronEnergy.Write();
  root1DHistogramOfElectronsTotalEnergy.Write();
}
//void Analysis::Run::fillIonAndElectronBasicData() {
//
//}
void Analysis::Run::fillIonAndElectronMomentumData() {
  const double &iSumOfTOF = pIons->getSumOfTOF(*pUnit);
  const double &iTotalE = pIons->getEnergy(*pUnit);
  const double &e1HitE = pElectrons->getRealOrDummyObject(0).getEnergy(*pUnit);
  root2DHistogramOfSumOfIonTOF_1stHitElectronEnergy.Fill(iSumOfTOF, e1HitE);
  root2DHistogramOfTotalEnergy_1stHitElectronEnergy.Fill(iTotalE, e1HitE);
}
void Analysis::Run::writeIonAndElectronMomentumData() {
  root2DHistogramOfSumOfIonTOF_1stHitElectronEnergy.Write();
  root2DHistogramOfTotalEnergy_1stHitElectronEnergy.Write();
}
const size_t &Analysis::Run::getEntries() const {
  return (const size_t &) pChain->GetEntries();
}
