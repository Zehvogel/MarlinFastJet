#ifndef FASTJETUTIL_H
#define FASTJETUTIL_H 1

class FastJetProcessor;

#include "FastJetProcessor.h"
#include "EClusterMode.h"

#include "LCIOSTLTypes.h"
#include "marlin/Processor.h"

//FastJet
#include <fastjet/SISConePlugin.hh>
#include <fastjet/SISConeSphericalPlugin.hh>
#include <fastjet/CDFMidPointPlugin.hh>
#include <fastjet/CDFJetCluPlugin.hh>
#include <fastjet/NestedDefsPlugin.hh>
#include <fastjet/EECambridgePlugin.hh>
#include <fastjet/JadePlugin.hh>

#include <fastjet/contrib/ValenciaPlugin.hh>

#include <stdexcept>

#define ITERATIVE_INCLUSIVE_MAX_ITERATIONS 20


class SkippedFixedNrJetException: public std::runtime_error {
public:
  SkippedFixedNrJetException():std::runtime_error("") {}
};

#include <string>

class FastJetUtil {

public:
  FastJetUtil(): _cs(NULL),
		 _jetAlgoNameAndParams( EVENT::StringVec() ),
		 _jetAlgoName(""),
		 _jetAlgo(NULL),
		 _jetAlgoType(),
		 _clusterModeNameAndParam( EVENT::StringVec() ),
		 _clusterModeName(""),
		 _clusterMode(EClusterMode::NONE),
		 _jetRecoSchemeName(""),
		 _jetRecoScheme(),
		 _strategyName(""),
		 _strategy(),
		 _requestedNumberOfJets(0),
		 _yCut(0.0),
		 _minPt(0.0),
		 _minE(0.0)
  {}


  FastJetUtil(const FastJetUtil& rhs):
    _cs(new ClusterSequence(*(rhs._cs))),
    _jetAlgoNameAndParams( rhs._jetAlgoNameAndParams ),
    _jetAlgoName(rhs._jetAlgoName),
    _jetAlgo(new fastjet::JetDefinition(*(rhs._jetAlgo))),
    _jetAlgoType(rhs._jetAlgoType),
    _clusterModeNameAndParam(rhs._clusterModeNameAndParam),
    _clusterModeName(rhs._clusterModeName),
    _clusterMode(rhs._clusterMode),
    _jetRecoSchemeName(rhs._jetRecoSchemeName),
    _jetRecoScheme(rhs._jetRecoScheme),
    _strategyName(rhs._strategyName),
    _strategy(rhs._strategy),
    _requestedNumberOfJets(rhs._requestedNumberOfJets),
    _yCut(rhs._yCut),
    _minPt(rhs._minPt),
    _minE(rhs._minE)
  {}

  FastJetUtil& operator=(const FastJetUtil& rhs) {
    if( this == &rhs ){
      return *this;
    }
    delete this->_cs;
    this->_cs = new ClusterSequence(*(rhs._cs));
    delete this->_jetAlgo;
    this->_jetAlgo = new fastjet::JetDefinition(*rhs._jetAlgo);
    return *this;
  }


  ~FastJetUtil() {
    delete _cs;
    delete _jetAlgo;
  }

public:

  ClusterSequence *_cs;

  // jet algorithm
  EVENT::StringVec _jetAlgoNameAndParams;
  std::string _jetAlgoName;
  fastjet::JetDefinition* _jetAlgo;
  fastjet::JetAlgorithm _jetAlgoType;

  // clustering mode
  EVENT::StringVec _clusterModeNameAndParam;
  std::string _clusterModeName;
  EClusterMode _clusterMode;

  // jet reco scheme
  std::string _jetRecoSchemeName;
  fastjet::RecombinationScheme	_jetRecoScheme;

  // jet strategy
  std::string _strategyName;
  fastjet::Strategy _strategy;

  // parameters
  unsigned _requestedNumberOfJets;
  double _yCut;
  double _minPt;
  double _minE;

public:
  /// call in processor constructor (c'tor) to register parameters
  inline void registerFastJetParameters(FastJetProcessor* proc);
  /// call in processor init
  inline void init();
  /// convert reconstructed particles to pseudo jets
  inline PseudoJetList convertFromRecParticle(LCCollection* recCol);
  /// does the actual clustering
  inline std::vector< fastjet::PseudoJet > clusterJets(PseudoJetList& pjList, LCCollection* reconstructedPars);

protected:
  // helper functions to init the jet algorithms in general
  void initJetAlgo();
  void initRecoScheme();
  void initStrategy();
  void initClusterMode();
  inline bool isJetAlgo(string algo, int nrParams, int supportedModes);

  // special clustering function, called from clusterJets
  vector < fastjet::PseudoJet > doIterativeInclusiveClustering(PseudoJetList& pjList);

}; //end class FastJetUtil


void FastJetUtil::registerFastJetParameters( FastJetProcessor* proc ) {

  EVENT::StringVec defAlgoAndParam;
  defAlgoAndParam.push_back("kt_algorithm");
  defAlgoAndParam.push_back("0.7");
  proc->registerProcessorParameter(
				   "algorithm",
				   "Selects the algorithm and its parameters. E.g. 'kt_algorithm 0.7' or 'ee_kt_algorithm'. For a full list of supported algorithms, see the logfile after execution.",
				   _jetAlgoNameAndParams,
				   defAlgoAndParam);

  proc->registerProcessorParameter(
				   "recombinationScheme",
				   "The recombination scheme used when merging 2 particles. Usually there is no need to use anything else than 4-Vector addition: E_scheme",
				   _jetRecoSchemeName,
				   std::string("E_scheme"));

  EVENT::StringVec defClusterMode;
  defClusterMode.push_back("Inclusive");
  proc->registerProcessorParameter(
				   "clusteringMode",
				   "One of 'Inclusive <minPt>', 'InclusiveIterativeNJets <nrJets> <minE>', 'ExclusiveNJets <nrJets>', 'ExclusiveYCut <yCut>'. Note: not all modes are available for all algorithms.",
				   _clusterModeNameAndParam,
				   defClusterMode);


}

void FastJetUtil::init() {

  initStrategy();
  initRecoScheme();
  initClusterMode();
  initJetAlgo();

}

void FastJetUtil::initJetAlgo() {

  // parse the given jet algorithm string

  // sanity check
  if (_jetAlgoNameAndParams.size() < 1)
    throw Exception("No Jet algorithm provided!");

  // save the name
  this->_jetAlgoName = _jetAlgoNameAndParams[0];

  // check all supported algorithms and create the appropriate FJ instance

  _jetAlgo = NULL;
  streamlog_out(MESSAGE) << "Algorithms: ";	// the isJetAlgo function will write to streamlog_out(MESSAGE), so that we get a list of available algorithms in the log

  // example: kt_algorithm, needs 1 parameter, supports inclusive, inclusiveIterative, exlusiveNJets and exlusiveYCut clustering
  if (isJetAlgo("kt_algorithm", 1, FJ_inclusive | FJ_exclusive_nJets | FJ_exclusive_yCut | OWN_inclusiveIteration)) {
    _jetAlgoType = kt_algorithm;
    _jetAlgo = new fastjet::JetDefinition(
					  _jetAlgoType, atof(_jetAlgoNameAndParams[1].c_str()), _jetRecoScheme, _strategy);

  }

  if (isJetAlgo("cambridge_algorithm", 1, FJ_inclusive | FJ_exclusive_nJets | FJ_exclusive_yCut | OWN_inclusiveIteration)) {
    _jetAlgoType = cambridge_algorithm;
    _jetAlgo = new fastjet::JetDefinition(
					  _jetAlgoType, atof(_jetAlgoNameAndParams[1].c_str()), _jetRecoScheme, _strategy);
  }

  if (isJetAlgo("antikt_algorithm", 1, FJ_inclusive | OWN_inclusiveIteration)) {
    _jetAlgoType = antikt_algorithm;
    _jetAlgo = new fastjet::JetDefinition(
					  _jetAlgoType, atof(_jetAlgoNameAndParams[1].c_str()), _jetRecoScheme, _strategy);
  }

  if (isJetAlgo("genkt_algorithm", 2, FJ_inclusive | OWN_inclusiveIteration | FJ_exclusive_nJets | FJ_exclusive_yCut)) {
    _jetAlgoType = genkt_algorithm;
    _jetAlgo = new fastjet::JetDefinition(
					  _jetAlgoType, atof(_jetAlgoNameAndParams[1].c_str()), atof(_jetAlgoNameAndParams[2].c_str()), _jetRecoScheme, _strategy);
  }

  if (isJetAlgo("cambridge_for_passive_algorithm", 1, FJ_inclusive | OWN_inclusiveIteration | FJ_exclusive_nJets | FJ_exclusive_yCut)) {
    _jetAlgoType = cambridge_for_passive_algorithm;
    _jetAlgo = new fastjet::JetDefinition(
					  _jetAlgoType, atof(_jetAlgoNameAndParams[1].c_str()), _jetRecoScheme, _strategy);
  }

  if (isJetAlgo("genkt_for_passive_algorithm", 1, FJ_inclusive | OWN_inclusiveIteration)) {
    _jetAlgoType = genkt_for_passive_algorithm;
    _jetAlgo = new fastjet::JetDefinition(
					  _jetAlgoType, atof(_jetAlgoNameAndParams[1].c_str()), _jetRecoScheme, _strategy);
  }

  if (isJetAlgo("ee_kt_algorithm", 0, FJ_exclusive_nJets | FJ_exclusive_yCut)) {
    _jetAlgoType = ee_kt_algorithm;
    _jetAlgo = new fastjet::JetDefinition(
					  _jetAlgoType, _jetRecoScheme, _strategy);
  }

  if (isJetAlgo("ee_genkt_algorithm", 1, FJ_exclusive_nJets | FJ_exclusive_yCut)) {
    _jetAlgoType = ee_genkt_algorithm;
    _jetAlgo = new fastjet::JetDefinition(
					  _jetAlgoType, atof(_jetAlgoNameAndParams[1].c_str()), _jetRecoScheme, _strategy);
  }

  if (isJetAlgo("SISConePlugin", 2, FJ_inclusive | OWN_inclusiveIteration)) {
    fastjet::SISConePlugin* pl;

    pl = new fastjet::SISConePlugin(
				    atof(_jetAlgoNameAndParams[1].c_str()),
				    atof(_jetAlgoNameAndParams[2].c_str())
				    );
    _jetAlgo = new fastjet::JetDefinition(pl);
  }

  if (isJetAlgo("SISConeSphericalPlugin", 2, FJ_inclusive | OWN_inclusiveIteration)) {

    fastjet::SISConeSphericalPlugin* pl;
    pl = new fastjet::SISConeSphericalPlugin(
					     atof(_jetAlgoNameAndParams[1].c_str()),
					     atof(_jetAlgoNameAndParams[2].c_str())
					     );

    _jetAlgo = new fastjet::JetDefinition(pl);
  }

  if (isJetAlgo("ValenciaPlugin", 3, FJ_exclusive_nJets | FJ_exclusive_yCut)) {

    fastjet::contrib::ValenciaPlugin* pl;
    pl = new fastjet::contrib::ValenciaPlugin(
					      atof(_jetAlgoNameAndParams[1].c_str()),  // R value
					      atof(_jetAlgoNameAndParams[2].c_str()),  // beta value
					      atof(_jetAlgoNameAndParams[3].c_str())   // gamma value
					      );

    _jetAlgo = new fastjet::JetDefinition(pl);
  }

  // TODO: Maybe we should complete the user defined (ATLAS, CMS, ..) algorithms
  //	if (isJetAlgo("ATLASConePlugin", 1))
  //	{
  //		fastjet::ATLASConePlugin* pl;
  //		pl = new fastjet::ATLASConePlugin(
  //				atof(_jetAlgoAndParamsString[1].c_str())
  //				);
  //
  //		_jetAlgo = new fastjet::JetDefinition(pl);
  //	}
  //
  //	if (isJetAlgo("CMSIterativeConePlugin", 1))
  //	{
  //		fastjet::CMSIterativeConePlugin* pl;
  //		pl = new fastjet::CMSIterativeConePlugin(
  //				atof(_jetAlgoAndParamsString[1].c_str())
  //				);
  //
  //		_jetAlgo = new fastjet::JetDefinition(pl);
  //	}

  streamlog_out(MESSAGE) << endl; // end of list of available algorithms

  if (!_jetAlgo) {
    streamlog_out(ERROR) << "The given algorithm \"" << _jetAlgoName
			 << "\" is unknown to me!" << endl;
    throw Exception("Unknown FastJet algorithm.");
  }

  streamlog_out(MESSAGE) << "jet algorithm: " << _jetAlgo->description() << endl;
}


bool FastJetUtil::isJetAlgo(string algo, int nrParams, int supportedModes)
{
  streamlog_out(MESSAGE) << " " << algo;

  // check if the chosen algorithm is the same as it was passed
  if (_jetAlgoName.compare(algo) != 0) {
    return false;
  }
  streamlog_out(MESSAGE) << "*"; // mark the current algorithm as the selected
				 // one, even before we did our checks on nr of
				 // parameters etc.

  // check if we have enough number of parameters
  if ((int)_jetAlgoNameAndParams.size() - 1 != nrParams) {
    streamlog_out(ERROR) << endl
			 << "Wrong numbers of parameters for algorithm: " << algo << endl
			 << "We need " << nrParams << " params, but we got " << _jetAlgoNameAndParams.size() - 1 << endl;
    throw Exception("You have insufficient number of parameters for this algorithm! See log for more details.");
  }

  // check if the mode is supported via a binary AND
  if ((supportedModes & _clusterMode) != _clusterMode) {
    streamlog_out(ERROR) << endl
			 << "This algorithm is not capable of running in this clustering mode ("
			 << _clusterMode << "). Sorry!" << endl;
    throw Exception("This algorithm is not capable of running in this mode");
  }

  return true;
}

/// parse the steering parameter for the recombination scheme
void FastJetUtil::initRecoScheme() {

  if (_jetRecoSchemeName.compare("E_scheme") == 0)
    _jetRecoScheme = fastjet::E_scheme;
  else if (_jetRecoSchemeName.compare("pt_scheme") == 0)
    _jetRecoScheme = fastjet::pt_scheme;
  else if (_jetRecoSchemeName.compare("pt2_scheme") == 0)
    _jetRecoScheme = fastjet::pt2_scheme;
  else if (_jetRecoSchemeName.compare("Et_scheme") == 0)
    _jetRecoScheme = fastjet::Et_scheme;
  else if (_jetRecoSchemeName.compare("Et2_scheme") == 0)
    _jetRecoScheme = fastjet::Et2_scheme;
  else if (_jetRecoSchemeName.compare("BIpt_scheme") == 0)
    _jetRecoScheme = fastjet::BIpt_scheme;
  else if (_jetRecoSchemeName.compare("BIpt2_scheme") == 0)
    _jetRecoScheme = fastjet::BIpt2_scheme;
  else {
      streamlog_out(ERROR)
	<< "Unknown recombination scheme: " << _jetRecoSchemeName << endl;
      throw Exception("Unknown FastJet recombination scheme! See log for more details.");
  }

  streamlog_out(MESSAGE) << "recombination scheme: " << _jetRecoSchemeName << endl;
}

void FastJetUtil::initStrategy() {
  // we could provide a steering parameter for this and it would be parsed here.
  // however, when using the 'Best' clustering strategy FJ will chose automatically from a big list
  // changing this would (most likely) only change the speed of calculation, not the outcome
  _strategy = fastjet::Best;
  _strategyName = "Best";
  streamlog_out(MESSAGE) << "Strategy: " << _strategyName << endl;
}

/// parse the clustermode string
void FastJetUtil::initClusterMode() {

  // at least a name has to be given
  if (_clusterModeNameAndParam.size() == 0)
    throw Exception("Cluster mode not specified");

  // save the name of the cluster mode
  _clusterModeName = _clusterModeNameAndParam[0];

  _clusterMode = NONE;

  // check the different cluster mode possibilities, and check if the number of parameters are correct
  if (_clusterModeName.compare("Inclusive") == 0) {

    if (_clusterModeNameAndParam.size() != 2) {
      throw Exception("Wrong Parameter(s) for Clustering Mode. Expected:\n <parameter name=\"clusteringMode\" type=\"StringVec\"> Inclusive <minPt> </parameter>");
    }

    _minPt = atof(_clusterModeNameAndParam[1].c_str());
    _clusterMode = FJ_inclusive;

  } else if (_clusterModeName.compare("InclusiveIterativeNJets") == 0) {

    if (_clusterModeNameAndParam.size() != 3) {
      throw Exception("Wrong Parameter(s) for Clustering Mode. Expected:\n <parameter name=\"clusteringMode\" type=\"StringVec\"> InclusiveIterativeNJets <NJets> <minE> </parameter>");
    }

    _requestedNumberOfJets = atoi(_clusterModeNameAndParam[1].c_str());
    _minE = atoi(_clusterModeNameAndParam[2].c_str());
    _clusterMode = OWN_inclusiveIteration;

  }  else if (_clusterModeName.compare("ExclusiveNJets") == 0) {

    if (_clusterModeNameAndParam.size() != 2) {
      throw Exception("Wrong Parameter(s) for Clustering Mode. Expected:\n <parameter name=\"clusteringMode\" type=\"StringVec\"> ExclusiveNJets <NJets> </parameter>");
    }

    _requestedNumberOfJets = atoi(_clusterModeNameAndParam[1].c_str());
    _clusterMode = FJ_exclusive_nJets;

  } else if (_clusterModeName.compare("ExclusiveYCut") == 0) {

    if (_clusterModeNameAndParam.size() != 2) {
      throw Exception("Wrong Parameter(s) for Clustering Mode. Expected:\n <parameter name=\"clusteringMode\" type=\"StringVec\"> ExclusiveYCut <yCut> </parameter>");
    }

    _yCut = atof(_clusterModeNameAndParam[1].c_str());
    _clusterMode = FJ_exclusive_yCut;

  } else {
    throw Exception("Unknown cluster mode.");
  }

  streamlog_out(MESSAGE) << "cluster mode: " << _clusterMode << endl;
}

std::vector< fastjet::PseudoJet > FastJetUtil::clusterJets( PseudoJetList& pjList, LCCollection* reconstructedPars) {
  ///////////////////////////////
  // do the jet finding for the user defined parameter jet finder
  _cs = new ClusterSequence(pjList, *_jetAlgo);

  std::vector< fastjet::PseudoJet > jets; // will contain the found jets

  if (_clusterMode == FJ_inclusive) {

    jets = _cs->inclusive_jets(_minPt);

  } else if (_clusterMode == FJ_exclusive_yCut) {

    jets = _cs->exclusive_jets_ycut(_yCut);

  } else if (_clusterMode == FJ_exclusive_nJets) {

    // sanity check: if we have not enough particles, FJ will cause an assert
    if (reconstructedPars->getNumberOfElements() < (int)_requestedNumberOfJets) {

      streamlog_out(WARNING) << "Not enough elements in the input collection to create " << _requestedNumberOfJets << " jets." << endl;
      throw SkippedFixedNrJetException();

    } else {

      jets = _cs->exclusive_jets((int)(_requestedNumberOfJets));

    }

  } else if (_clusterMode == OWN_inclusiveIteration) {

    // sanity check: if we have not enough particles, FJ will cause an assert
    if (reconstructedPars->getNumberOfElements() < (int)_requestedNumberOfJets) {

      streamlog_out(WARNING) << "Not enough elements in the input collection to create " << _requestedNumberOfJets << " jets." << endl;
      throw SkippedFixedNrJetException();

    } else {

      jets = doIterativeInclusiveClustering(pjList);

    }

  }

  return jets;

}


PseudoJetList FastJetUtil::convertFromRecParticle(LCCollection* recCol) {
  // foreach RecoParticle in the LCCollection: convert it into a PseudoJet and and save it in our list
  PseudoJetList pjList;
  for (int i = 0; i < recCol->getNumberOfElements(); ++i) {
    ReconstructedParticle* par = static_cast<ReconstructedParticle*> (recCol->getElementAt(i));
    pjList.push_back( fastjet::PseudoJet( par->getMomentum()[0],
					   par->getMomentum()[1],
					   par->getMomentum()[2],
					   par->getEnergy() ) );
    pjList.back().set_user_index(i);	// save the id of this recParticle
  }

  return pjList;
}

vector < fastjet::PseudoJet > FastJetUtil::doIterativeInclusiveClustering( PseudoJetList& pjList) {
  // lets do a iterative procedure until we found the correct number of jets
  // for that we will do inclusive clustering, modifying the R parameter in some kind of minimization
  // this is based on Marco Battaglia's FastJetClustering

  double R = M_PI_4;	// maximum of R is Pi/2, minimum is 0. So we start hat Pi/4
  double RDiff = R / 2;	// the step size we modify the R parameter at each iteration. Its size for the n-th step is R/(2n), i.e. starts with R/2
  vector< fastjet::PseudoJet > jets;
  vector< fastjet::PseudoJet > jetsReturn;
  unsigned nJets;
  int iIter = 0;	// nr of current iteration

  // these variables are only used if the SisCone(Spherical)Plugin is selected
  // This is necessary, as these are plugins and hence use a different constructor than
  // the built in fastjet algorithms

  // here we save pointer to the plugins, so that if they are created, we can delete them again after usage
  fastjet::SISConePlugin* pluginSisCone = NULL;
  fastjet::SISConeSphericalPlugin* pluginSisConeSph = NULL;
  // check if we use the siscones
  bool useSisCone = _jetAlgoName.compare("SISConePlugin") == 0;
  bool useSisConeSph = _jetAlgoName.compare("SISConeSphericalPlugin") == 0;
  // save the 2nd parameter of the SisCones
  double sisConeOverlapThreshold = 0;
  if (useSisCone || useSisConeSph)
    sisConeOverlapThreshold = atof(_jetAlgoNameAndParams[2].c_str());

  // do a maximum of N iterations
  // For each iteration we will modify the Radius R by RDiff. RDiff will be reduced by 2 for each iteration
  // i.e. RDiff = Pi/8, Pi/16, Pi/32, Pi/64, ...
  // e.g.
  // R = Pi/4
  // R = Pi/4 + Pi/8
  // R = Pi/4 + Pi/8 - Pi/16
  // R = Pi/4 + Pi/8 - Pi/16 + Pi/32
  // ...
  for (iIter=0; iIter<ITERATIVE_INCLUSIVE_MAX_ITERATIONS; iIter++) {

    // do the clustering for this value of R. For this we need to re-initialize the JetDefinition, as it takes the R parameter
    JetDefinition* jetDefinition = NULL;

    // unfortunately SisCone(spherical) are being initialized differently, so we have to check for this
    if (useSisCone) {
      pluginSisCone = new fastjet::SISConePlugin(R, sisConeOverlapThreshold);
      jetDefinition = new fastjet::JetDefinition(pluginSisCone);
    } else if (useSisConeSph) {
      pluginSisConeSph = new fastjet::SISConeSphericalPlugin(R, sisConeOverlapThreshold);
      jetDefinition = new fastjet::JetDefinition(pluginSisConeSph);
    } else {
      jetDefinition = new JetDefinition(_jetAlgoType, R, _jetRecoScheme, _strategy);
    }

    // now we can finally create the cluster sequence
    ClusterSequence cs(pjList, *jetDefinition);

    jets = cs.inclusive_jets(0);	// no pt cut, we will do an energy cut
    jetsReturn.clear();

    // count the number of jets above threshold
    nJets = 0;
    for (unsigned j=0; j<jets.size(); j++)
      if (jets[j].E() > _minE)
	jetsReturn.push_back(jets[j]);
    nJets = jetsReturn.size();

    streamlog_out(DEBUG) << iIter << " " << R << " " << jets.size() << " " << nJets << endl;

    if (nJets == _requestedNumberOfJets) { // if the number of jets is correct: success!
	break;

    } else if (nJets < _requestedNumberOfJets) {
	// if number of jets is too small: we need a smaller Radius per jet (so
	// that we get more jets)
	R -= RDiff;

    } else if (nJets > _requestedNumberOfJets) {	// if the number of jets is too
						// high: increase the Radius
      R += RDiff;
    }

    RDiff /= 2;

    // clean up
    delete pluginSisCone; pluginSisCone = NULL;
    delete pluginSisConeSph; pluginSisConeSph = NULL;
    delete jetDefinition;

  }

  if (iIter == ITERATIVE_INCLUSIVE_MAX_ITERATIONS) {
    streamlog_out(WARNING) << "Maximum number of iterations reached. Canceling" << endl;
    throw SkippedFixedNrJetException();
    // Currently we will return the latest results, independent if the number is actually matched
    // jetsReturn.clear();
  }

  return jetsReturn;
}


#endif // FastJetUtil_h
