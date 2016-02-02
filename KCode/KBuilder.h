#ifndef KBUILDER_H
#define KBUILDER_H

#ifndef NtupleClass_cxx
#define NtupleClass_cxx

//custom headers
#include "KMap.h"
#include "KMath.h"
#include "KParser.h"
#include "KBase.h"
#include "NtupleClass.h"
#include "KSelection.h"
#include "../corrections/GetTriggerEffCorr.C"
#include "../corrections/ISRCorrector.h"

//ROOT headers
#include <TROOT.h>
#include <TFile.h>
#include <TH1.h>
#include <TProfile.h>
#include <TH2.h>
#include <TStyle.h>
#include <TLorentzVector.h>
#include <TMath.h>

//STL headers
#include <vector>
#include <string>
#include <iostream>
#include <map>

using namespace std;

void NtupleClass::Loop() {}

//---------------------------------------------------------------
//histo builder class - loops over tree to fill histos for a base
class KBuilder : public NtupleClass {
	public:
		//constructors
		KBuilder() : NtupleClass(), MyBase(0), localOpt(0), globalOpt(0), MySelection(0) {
			//must always have local & global option maps
			if(localOpt==0) localOpt = new OptionMap();
			if(globalOpt==0) globalOpt = new OptionMap();
		}
		KBuilder(KBase* MyBase_) : 
			NtupleClass(MyBase_->GetTree()), MyBase(MyBase_), localOpt(MyBase->GetLocalOpt()), globalOpt(MyBase->GetGlobalOpt()), MySelection(MyBase->GetSelection())
		{
			//must always have local & global option maps
			if(localOpt==0) localOpt = new OptionMap();
			if(globalOpt==0) globalOpt = new OptionMap();
			//pass self to selection; also sets looper for selectors, variation, variators
			MySelection->SetLooper(this); 
		}
		//destructor
		virtual ~KBuilder() {}

		//functions for histo creation
		using NtupleClass::Cut;
		virtual bool Cut() { //this implements the set of cuts common between data and MC
			bool goodEvent = true;
		
			return (goodEvent && MySelection->DoSelection());
		}
		virtual double GetWeight() { return 1.; }
		virtual void Loop() {
			if (fChain == 0) return;
			
			//initial loop to get histo variables
			int table_size = MyBase->GetTable().size();
			vars.clear(); vars.reserve(table_size);
			htmp.clear(); htmp.reserve(table_size);
			HMit sit;
			for(sit = MyBase->GetTable().begin(); sit != MyBase->GetTable().end(); sit++){
				//get histo name
				string stmp = sit->first;
				htmp.push_back(sit->second);
				//split up histo variable names
				vector<string> vars_tmp;
				KParser::process(stmp,'_',vars_tmp);
				vars.push_back(vars_tmp);
			}
			
			//check for branches to enable/disable
			vector<string> disable_branches;
			globalOpt->Get("disable_branches",disable_branches);
			for(unsigned b = 0; b < disable_branches.size(); ++b){
				fChain->SetBranchStatus(disable_branches[b].c_str(),0);
			}
			vector<string> enable_branches;
			globalOpt->Get("enable_branches",enable_branches);
			for(unsigned b = 0; b < enable_branches.size(); ++b){
				fChain->SetBranchStatus(enable_branches[b].c_str(),1);
			}
			//check for any necessary branches
			CheckBranches();
			MySelection->CheckBranches();
			
			//loop over ntuple tree
			Long64_t nentries = fChain->GetEntries();
			int maxevents = 0;
			if(globalOpt->Get("maxevents",maxevents) && maxevents < nentries) nentries = maxevents;
			bool debugloop = globalOpt->Get("debugloop",false);
			Long64_t nbytes = 0, nb = 0;
			for (Long64_t jentry=0; jentry<nentries;jentry++) {
				Long64_t ientry = LoadTree(jentry);
				if (ientry < 0) break;
				nb = fChain->GetEntry(jentry);   nbytes += nb;
				if(debugloop && jentry % 10000 == 0) cout << MyBase->GetName() << " " << jentry << "/" << nentries << endl;
				
				Cut();
			}
			
			//final steps
			if(globalOpt->Get("debugcut",false)) {
				cout << MyBase->GetName() << endl;
				MySelection->PrintEfficiency(MySelection->GetSelectorWidth(),nentries);
			}
			
			if(globalOpt->Get("plotoverflow",false)){
				for(unsigned h = 0; h < htmp.size(); h++){
					if(vars[h].size()==2) continue; //not implemented for 2D histos or profiles yet
					
					//temporary histo to calculate error correctly when adding overflow bin to last bin
					TH1* otmp = (TH1*)htmp[h]->Clone();
					otmp->Reset("ICEM");
					int ovbin = htmp[h]->GetNbinsX()+1;
					double err = 0.;
					otmp->SetBinContent(ovbin-1,htmp[h]->IntegralAndError(ovbin,ovbin,err));
					otmp->SetBinError(ovbin-1,err);
					
					//add overflow bin to last bin
					htmp[h]->Add(otmp);
					
					//remove overflow bin from htmp[h] (for consistent integral/yield)
					htmp[h]->SetBinContent(ovbin,0);
					htmp[h]->SetBinError(ovbin,0);
					
					delete otmp;
				}
			}
		}
		//unimplemented
		virtual void CheckBranches() {}

	public:
		//member variables
		KBase* MyBase;
		OptionMap* localOpt;
		OptionMap* globalOpt;
		KSelection<KBuilder>* MySelection;
		vector<vector<string> > vars;
		vector<TH1*> htmp;
};

void KBase::Build(){
	if(!isBuilt) {
		if(MyBuilder==0) MyBuilder = new KBuilder(this);
		MyBuilder->Loop(); //loop over tree to build histograms
		isBuilt = true;
	}
}

//---------------------------------------------------------
//extension of builder class for data - has blinding option
class KBuilderData : public KBuilder {
	public:
		//constructors
		KBuilderData() : KBuilder() { }
		KBuilderData(KBase* MyBase_) : KBuilder(MyBase_) {
			//get options
			blind = globalOpt->Get("blind",false);
		}
		//destructor
		virtual ~KBuilderData() {}
		
		//functions for histo creation
		using NtupleClass::Cut;
		bool Cut(){
			bool goodEvent = true;
			
			//special blinding option for data (disabled by default)
			if(blind){
				//do not look at signal region
				//could make this setting into a double value for variable blinding...
			}
			
			//KBuilder::Cut() comes *last* because it includes histo filling selector
			return goodEvent ? KBuilder::Cut() : goodEvent;
		}
		
		//member variables
		bool blind;
};

void KBaseData::Build(){
	if(!isBuilt) {
		if(MyBuilder==0) MyBuilder = new KBuilderData(this);
		MyBuilder->Loop(); //loop over tree to build histograms
		isBuilt = true;
	}
}

//------------------------------------------------------------------------------------------------------------
//extension of builder class for MC - has weighting (corrections & normalization), extra cuts (fake tau, etc.)
class KBuilderMC : public KBuilder {
	public:
		//constructors
		KBuilderMC() : KBuilder() { }
		KBuilderMC(KBase* MyBase_) : KBuilder(MyBase_) { 
			//standard weight options
			normtype = ""; localOpt->Get("normtype",normtype); GetNormTypeEnum();
			unweighted = localOpt->Get("unweighted",false);
			useTreeWeight = globalOpt->Get("useTreeWeight",false);
			nEventProc = 0; got_nEventProc = localOpt->Get("nEventProc",nEventProc);
			xsection = 0; got_xsection = localOpt->Get("xsection",xsection);
			norm = 0; got_luminorm = globalOpt->Get("luminorm",norm);
			debugWeight = globalOpt->Get("debugWeight",false); didDebugWeight = false;
			
			//PU options
			pucorr = globalOpt->Get("pucorr",false);
			puunc = 0; globalOpt->Get("puunc",puunc);
			puhist = NULL; globalOpt->Get("puhist",puhist);
			puhistUp = NULL; globalOpt->Get("puhistUp",puhistUp);
			puhistDown = NULL; globalOpt->Get("puhistDown",puhistDown);
			
			//trig corr options
			trigcorr = globalOpt->Get("trigcorr",false);
			trigStatUnc = 0; globalOpt->Get("trigStatUnc", trigStatUnc);
			trigSystUnc = 0; globalOpt->Get("trigSystUnc", trigSystUnc);
			realMET = localOpt->Get("realMET",true);
			signal = localOpt->Get("signal",false);
			
			//ISR corr options
			isrcorr = globalOpt->Get("isrcorr",false);
			localOpt->Get("mother",mother);
			isrunc = 0; globalOpt->Get("isrunc", isrunc);
			if(isrcorr){
				TH1* isrtmp = NULL;
				if(isrunc==1) globalOpt->Get("isrhistUp",isrtmp);
				else if(isrunc==-1) globalOpt->Get("isrhistDown",isrtmp);
				else globalOpt->Get("isrhist",isrtmp);
				isrcorror.SetMother(mother);
				isrcorror.SetWeights(isrtmp,(TH1*)MyBase->GetFile()->Get("GenPt"));
			}
			
			//other uncertainty options
			fastsim = localOpt->Get("fastsim",false);
			jetidcorr = globalOpt->Get("jetidcorr",false);
			pdfunc = 0; globalOpt->Get("pdfunc",pdfunc);
			scaleunc = 0; globalOpt->Get("scaleunc",scaleunc);
			if(pdfunc!=0 || scaleunc!=0){
				//get the normalizations for pdf/scale uncertainties
				TH1F* h_norm = (TH1F*)MyBase->GetFile()->Get("PDFNorm");
				pdfnorms = vector<double>(4,1.0);
				//0: PDF up, 1: PDF down, 2: scale up, 3: scale down
				if(h_norm){
					double nominal = h_norm->GetBinContent(1);
					for(unsigned n = 0; n < 4; ++n){
						//(bin in histo = index + 2)
						pdfnorms[n] = nominal/h_norm->GetBinContent(n+2);
					}
				}
			}
		}
		//destructor
		virtual ~KBuilderMC() {}
		
		//functions for histo creation
		virtual void CheckBranches(){
			//force enable branches needed for cuts/weights/etc.
			fChain->SetBranchStatus("Weight",1); //needed for negative weights even if useTreeWeight==false
			if(pucorr){
				fChain->SetBranchStatus("TrueNumInteractions",1);
				fChain->SetBranchStatus("NVtx",1);
				if(puunc==1) fChain->SetBranchStatus("puSysUp",1);
				else if(puunc==-1) fChain->SetBranchStatus("puSysDown",1);
				else fChain->SetBranchStatus("puWeight",1);
			}
			if(isrcorr){
				fChain->SetBranchStatus("genParticles",1);
				fChain->SetBranchStatus("genParticles_PDGid",1);
			}
			if(NTenum==ttbarLowHT || NTenum==ttbarLowHThad || NTenum==ttbarHighHT) fChain->SetBranchStatus("genHT",1);
			if(NTenum==ttbarLowHThad){
				fChain->SetBranchStatus("GenEls",1);
				fChain->SetBranchStatus("GenMus",1);
				fChain->SetBranchStatus("GenTaus",1);
			}
			if(pdfunc!=0){
				fChain->SetBranchStatus("PDFweights",1);
			}
			if(scaleunc!=0){
				fChain->SetBranchStatus("ScaleWeights",1);
			}
		}
		using NtupleClass::Cut;
		bool Cut(){
			bool goodEvent = true;
			
			//check normalization type here
			if(NTenum==ttbarLowHT) { goodEvent &= genHT < 600; }
			else if(NTenum==ttbarLowHThad) { goodEvent &= genHT < 600 && GenEls->size()==0 && GenMus->size()==0 && GenTaus->size()==0; }
			else if(NTenum==ttbarHighHT) { goodEvent &= genHT >= 600; }
		
			//KBuilder::Cut() comes *last* because it includes histo filling selector
			return goodEvent ? KBuilder::Cut() : goodEvent;
		}
		double GetWeight(){
			double w = 1.;
			if(unweighted) return w;
			
			//check option in case correction types are disabled globally
			//(enabled by default
			//(*disabled* until 2015 data is available)
			
			if(pucorr) {
				//use TreeMaker weights if no histo provided
				if(puunc==1){
					w *= puhistUp ? puhistUp->GetBinContent(puhistUp->GetXaxis()->FindBin(min(TrueNumInteractions,puhistUp->GetBinLowEdge(puhistUp->GetNbinsX()+1)))) : puSysUp;
				}
				else if(puunc==-1){
					w *= puhistDown ? puhistDown->GetBinContent(puhistDown->GetXaxis()->FindBin(min(TrueNumInteractions,puhistDown->GetBinLowEdge(puhistDown->GetNbinsX()+1)))) : puSysDown;
				}
				else {
					w *= puhist ? puhist->GetBinContent(puhist->GetXaxis()->FindBin(min(TrueNumInteractions,puhist->GetBinLowEdge(puhist->GetNbinsX()+1)))) : puWeight;
				}
			}
			
			if(trigcorr){
				w *= GetTriggerEffCorr(signal, MHT, realMET, trigStatUnc, trigSystUnc);
			}
			
			if(isrcorr){
				w *= isrcorror.GetCorrection(genParticles,genParticles_PDGid);
			}
			
			if(pdfunc!=0){
				if(pdfunc==1) w *= *(TMath::LocMax(PDFweights->begin(),PDFweights->end()))*pdfnorms[0];
				else if(pdfunc==-1) w *= *(TMath::LocMin(PDFweights->begin(),PDFweights->end()))*pdfnorms[1];
			}
			
			if(scaleunc!=0){
				vector<double> ScaleWeightsMod = *ScaleWeights;
				//remove unwanted variations
				if(ScaleWeightsMod.size()>7) ScaleWeightsMod.erase(ScaleWeightsMod.begin()+7);
				if(ScaleWeightsMod.size()>5) ScaleWeightsMod.erase(ScaleWeightsMod.begin()+5);
				if(ScaleWeightsMod.size()>0) ScaleWeightsMod.erase(ScaleWeightsMod.begin());
				
				if(scaleunc==1) w *= *(TMath::LocMax(ScaleWeightsMod.begin(),ScaleWeightsMod.end()))*pdfnorms[2];
				else if(scaleunc==-1) w *= *(TMath::LocMin(ScaleWeightsMod.begin(),ScaleWeightsMod.end()))*pdfnorms[3];
			}
			
			//correct for expected FullSim PFJetID efficiency
			if(jetidcorr && fastsim){
				w *= 0.99;
			}
			
			//now do scaling: norm*xsection/nevents
			if(useTreeWeight) w *= Weight;
			else if(got_nEventProc && nEventProc>0 && got_xsection){
				w *= xsection/nEventProc;
				//account for negative weight events
				if(Weight<0) w *= -1;
				
				//debugging
				if(debugWeight && !didDebugWeight){
					int oldprec = cout.precision(20);
					cout << MyBase->GetName() << endl;
					cout << "TreeMaker: " << fabs(Weight) << endl;
					cout << "    KCode: " << xsection/nEventProc << " = " << xsection << " / " << nEventProc << endl;
					didDebugWeight = true;
					cout.precision(oldprec);
				}
			}
			
			//use lumi norm (default)
			if(got_luminorm) w *= norm;
			
			return w;
		}
		
		//enum for normtypes
		enum normtypes { NoNT=0, ttbarLowHT=1, ttbarLowHThad=2, ttbarHighHT=3 };
		//convert normtype from string to enum for quicker compares
		void GetNormTypeEnum(){
			if(normtype=="ttbarLowHT") NTenum = ttbarLowHT;
			else if(normtype=="ttbarLowHThad") NTenum = ttbarLowHThad;
			else if(normtype=="ttbarHighHT") NTenum = ttbarHighHT;
			else NTenum = NoNT;
		}

		//member variables
		bool unweighted, got_nEventProc, got_xsection, got_luminorm, useTreeWeight, debugWeight, didDebugWeight;
		bool pucorr, trigcorr, isrcorr, realMET, signal, fastsim, jetidcorr;
		int puunc, pdfunc, isrunc, scaleunc, trigStatUnc, trigSystUnc;
		vector<int> mother;
		TH1 *puhist, *puhistUp, *puhistDown;
		vector<double> pdfnorms;
		string normtype;
		normtypes NTenum;
		int nEventProc;
		double xsection, norm;
		ISRCorrector isrcorror;
};

void KBaseMC::Build(){
	if(!isBuilt) {
		if(MyBuilder==0) MyBuilder = new KBuilderMC(this);
		MyBuilder->Loop(); //loop over tree to build histograms
		isBuilt = true;
	}
}

#endif
#endif