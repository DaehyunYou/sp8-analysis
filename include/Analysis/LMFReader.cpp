//
// Created by Daehyun You on 12/1/15.
//

#ifdef FOR_COBOLDPC2002
Analysis::LMFReader::LMFReader(const CDoubleArray &array) :pParameters(&array) {
  return;
}
Analysis::LMFReader::~LMFReader() { return; }
const double Analysis::LMFReader::getAt(const int &iTDC,
                                        const int &iCH,
                                        const int &iHit) const {
  const int &nCH = this->numberOfChannelsUsed;
  const int &nHit = this->numberOfHitsUsed;
  assert(iCH >= 0 && iCH < nCH);
  assert(iHit >= 0 && iHit < nHit);
  return this->getParameters().GetAt(iHit+iCH*(nHit+1)+iTDC*(nHit+1)*nCH);
}
const CDoubleArray &Analysis::LMFReader::getParameters() const {
  return *(this->pParameters);
}
const int &Analysis::LMFReader::getNumberOfTDCUsed() const {
  return this->numberOfTDCUsed;
}
const int &Analysis::LMFReader::getNumberOfChannelsUsed() const {
  return this->numberOfChannelsUsed;
}
const int &Analysis::LMFReader::getNumberOfHitsUsed() const {
  return this->numberOfHitsUsed;
}
#endif
