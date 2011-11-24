#include <assert.h>
#include <string>

#include "EventStore.h"
#include "TrackSelector.h"
#include "JetFinder.h"
#include "VertexFitterLCFI.h"
#include "VertexFinderTearDown.h"
#include "VertexFinderSuehara.h"

#include "TROOT.h"
#include "TInterpreter.h"
#include "TFile.h"
#include "TTree.h"
#include "TLorentzVector.h"
#include "TCut.h"
#include "TRandom.h"
#include "TNtuple.h"

#include "process.h"
#include "geometry.h"

using namespace lcfiplus;
using namespace lcfiplus::algoEtc;

namespace lcfiplus{

	void PrimaryVertexFinder::init(Parameters *param){
		Algorithm::init(param);

		_vertex = 0;

		string vcolname = param->get("PrimaryVertexCollectionName",string("PrimaryVertex"));
		Event::Instance()->Register(vcolname.c_str(), _vertex, EventStore::PERSIST);

		// default setting
		Event::Instance()->setDefaultPrimaryVertex(vcolname.c_str());

		// parameters...(should be exported)
		_secVtxCfg = new TrackSelectorConfig;

		/*
		_secVtxCfg->maxD0 = 50.;
		_secVtxCfg->maxZ0 = 50.;
		_secVtxCfg->maxInnermostHitRadius = 20.;
		_secVtxCfg->minVtxPlusFtdHits = 3;
		*/

		_secVtxCfg->maxD0 = param->get("PrimaryVertexFinder.TrackMaxD0", 20.);
		_secVtxCfg->maxZ0 = param->get("PrimaryVertexFinder.TrackMaxZ0", 20.);
		_secVtxCfg->maxInnermostHitRadius = param->get("PrimaryVertexFinder.TrackMaxInnermostHitRadius",20.);
		_secVtxCfg->minVtxPlusFtdHits = param->get("PrimaryVertexFinder.TrackMinVtxFtdHits", 5);

		_chi2th = param->get("PrimaryVertexFinder.Chi2Threshold", 25.);
	}

	void PrimaryVertexFinder::process(){
		Event *event = Event::Instance();

		// clearing old vertices
		if (_vertex->size()>0){delete (*_vertex)[0]; _vertex->clear();}

		// cut bad tracks
		TrackVec &tracks = event->getTracks();
		TrackVec passedTracks = TrackSelector() (tracks, *_secVtxCfg);

		// primary vertex finder
		Vertex * vtx = findPrimaryVertex(passedTracks,_chi2th);
		_vertex->push_back(vtx);
	}

	void PrimaryVertexFinder::end() {
		delete _secVtxCfg;
	}


	void BuildUpVertex::init(Parameters *param){
		Algorithm::init(param);

		// register collection
		string vcolname = param->get("BuildUpVertexCollectionName",string("BuildUpVertex"));
		Event::Instance()->Register(vcolname.c_str(), _vertices, EventStore::PERSIST);

		// configuration
		// track cut
		_secVtxCfg = new TrackSelectorConfig;

		_secVtxCfg->maxD0 = param->get("BuildUpVertex.TrackMaxD0", 10.);
		_secVtxCfg->maxZ0 = param->get("BuildUpVertex.TrackMaxZ0", 20.);
		_secVtxCfg->minPt = param->get("BuildUpVertex.TrackMinPt", 0.1);
		_secVtxCfg->maxInnermostHitRadius = 1e10;

		_secVtxCfg->maxD0Err = param->get("BuildUpVertex.TrackMaxD0Err", .1);
		_secVtxCfg->maxZ0Err = param->get("BuildUpVertex.TrackMaxZ0Err", .1);

		_secVtxCfg->minTpcHits = param->get("BuildUpVertex.TrackMinTpcHits", 20);
		_secVtxCfg->minFtdHits = param->get("BuildUpVertex.TrackMinFtdHits", 3);
		_secVtxCfg->minVtxHitsWithoutTpcFtd = param->get("BuildUpVertex.TrackMinVxdHits", 3);
		_secVtxCfg->minVtxPlusFtdHits = param->get("BuildUpVertex.TrackMinVxdFtdHits", 0);

		// buildup parameters
		_chi2thpri = param->get("BuildUpVertex.PrimaryChi2Threshold", 25.);
		_chi2thsec = param->get("BuildUpVertex.SecondaryChi2Threshold", 9.);
		_massth = param->get("BuildUpVertex.MassThreshold", 10.);
		_posth = param->get("BuildUpVertex.MinDistFromIP", 0.3);
		_chi2orderinglimit = param->get("BuildUpVertex.MaxChi2ForDistOrder", 1.0);

		_doassoc = param->get("BuildUpVertex.AssocIPTracks", 1);
		_minimumdist = param->get("BuildUpVertex.AssocIPTracksMinDist", 0.);
		_chi2ratio = param->get("BuildUpVertex.AssocIPTracksChi2RatioSecToPri", 2.0);
	}

	void BuildUpVertex::process(){
		Event *event = Event::Instance();

		// clearing old vertices
		for(unsigned int n=0;n<_vertices->size();n++)
			delete (*_vertices)[n];
		_vertices->clear();

		// cut bad tracks
		TrackVec &tracks = event->getTracks();
		TrackVec passedTracks = TrackSelector() (tracks, *_secVtxCfg);

		// build up vertexing
		VertexFinderSuehara::buildUp(passedTracks, *_vertices, _chi2thpri, _chi2thsec, _massth, _posth, _chi2orderinglimit);
		if(_doassoc)
			VertexFinderSuehara::associateIPTracks(*_vertices,_minimumdist, 0, _chi2ratio);
	}

	void BuildUpVertex::end(){
		delete _secVtxCfg;
	}

	void JetClustering::init(Parameters *param){
		Algorithm::init(param);

		_vcolname = param->get("JetClustering.InputVertexCollectionName",string("BuildUpVertex"));
		Event::Instance()->Get(_vcolname.c_str(), _vertices);
		if(!_vertices)
			cout << "JetClustering::init: vertex collection not found; will try later." << endl;

		string jcolname = param->get("JetClustering.OutputJetCollectionName",string("Jets"));
		Event::Instance()->Register(jcolname.c_str(), _jets, EventStore::PERSIST | EventStore::JET_EXTRACT_VERTEX);

		_njets = param->get("JetClustering.NJetsRequested",int(6));
		_ycut = param->get("JetClustering.YCut", double(0));

		_useMuonID = param->get("JetClustering.UseMuonID", int(1));

		_vsMinDist = param->get("JetClustering.VertexSelectionMinimumDistance", double(0.3));
		_vsMaxDist = param->get("JetClustering.VertexSelectionMaximumDistance", double(30.));
		_vsK0MassWidth = param->get("JetClustering.VertexSelectionK0MassWidth", double(0.02));
	}

	void JetClustering::process()
	{
		// clearing old jets
		for(unsigned int n=0;n<_jets->size();n++)
			delete (*_jets)[n];
		_jets->clear();

		if(!_vertices){
			// retry
			Event::Instance()->Get(_vcolname.c_str(), _vertices);
			if(!_vertices){
				cout << "JetClustering::Process: Vertex not found, clustering without vertices..." << endl;
			}
			else
				cout << "JetClustering::Process: Vertex found." << endl;
		}

		Event *event = Event::Instance();

		JetConfig jetCfg;
		jetCfg.nJet = _njets;
		JetFinder* jetFinder = new JetFinder(jetCfg);
		double ycut = _ycut;

		// select vertices
		vector<const Vertex *> selectedVertices;
		vector<const Track *> residualTracks = event->getTracks();
		if(_vertices){
			for(VertexVecIte it = _vertices->begin(); it != _vertices->end();it++){
				const Vertex *v = *it;
				double mass = 0.;
				double k0mass = .498;
				if(v->getTracks().size() == 2)
					mass = (*(TLorentzVector *)(v->getTracks()[0]) + *(TLorentzVector *)(v->getTracks()[1])).M();
				if((mass < k0mass - _vsK0MassWidth/2 || mass > k0mass + _vsK0MassWidth/2) && v->getPos().Mag() < _vsMaxDist && v->getPos().Mag() > _vsMinDist){
					selectedVertices.push_back(v);
					for(TrackVecIte itt = v->getTracks().begin(); itt != v->getTracks().end(); itt++){
						vector<const Track *>::iterator itt2 = remove_if(residualTracks.begin(), residualTracks.end(), bind2nd(equal_to<const Track *>(), *itt));
						residualTracks.erase(itt2, residualTracks.end());
					}
				}
			}
		}
		*_jets = jetFinder->run(residualTracks, event->getNeutrals(), selectedVertices, &ycut, _useMuonID);

		if(_vertices){
			cout << "JetClustering: _vertices.size() = " << _vertices->size() << endl;
			cout << "JetClustering: selectedVertices.size() = " << selectedVertices.size() << endl;
		}

/*
		Parameters testPid;
		testPid.add("testparapi", (double)3.14159);
		testPid.add("testpara", (double)2.7183);

		const vector<const Jet *> *testJets = constVector(_jets);

		double a = 4.4444;
		for (JetVecIte it = testJets->begin(); it != testJets->end(); ++it) {
			const Jet* jet = *it;
			testPid.assign("testpara",a);
			jet->addParam("testPID", testPid);

			a *= 2.;
			cout << testPid.get("testpara", double(0.)) << endl;

			cout << "   Jet nvtx = " << jet->getVertices().size() << endl;
		}
		*/
	}

	void JetClustering::end() {
		//cout << "ENDO" << endl;
	}


	/*
	void JetVertexRefiner::init() {
		Algorithm::init(param);

		string vcolname = param->get("JetVertexRefinerInputJetCollectionName",string("VertexJets"));
		Event::Instance()->Get(vcolname.c_str(), _inputJets);
		string jcolname = param->get("JetVertexRefinerOutputJetCollectionName",string("RefinedJets"));
		Event::Instance()->Register(jcolname.c_str(), _outputJets, EventStore::PERSIST | EventStore::JET_EXTRACT_VERTEX);
	}

	void JetVertexRefiner::process() {
		// clearing old jets
		for(unsigned int n=0;n<_outputJets->size();n++)
			delete (*_outputJets)[n];
		_outputJets->clear();

		Event *event = Event::Instance();

		TrackVec residualTracks = event->getTracks();
		for(JetVecIte it = _inputJets->begin(); it != _inputJets->end();++it){
			Jet* jet = *it;

			TrackVec & tracks = jet->getTracks();
			VertexVec & vertices = jet->getVertices();
			for (VertexVecIte it2 = vertices.begin(); it2 != vertices.end(); ++it2) {
			}
		}
	}
	*/
}

