#ifndef KBUILDERSELECTORS_H
#define KBUILDERSELECTORS_H

//custom headers
#include "KSelection.h"
#include "KBuilder.h"
#include "KMath.h"
#include "../btag/BTagCalibrationStandalone.cc"

//ROOT headers
#include <TROOT.h>
#include <TFile.h>
#include <TTree.h>
#include <TLorentzVector.h>

//STL headers
#include <string>
#include <vector>
#include <cstdlib>
#include <map>

using namespace std;

//base class for Selectors is in KSelection.h

//---------------------------------------------------------------
//class to store and apply RA2 binning
class KRA2BinSelector : public KSelector<KBuilder> {
	public:
		//constructor
		KRA2BinSelector() : KSelector<KBuilder>() { }
		KRA2BinSelector(string name_, OptionMap* localOpt_) : KSelector<KBuilder>(name_,localOpt_), RA2Exclusive(true), DoBTagSF(false) { }
		virtual void SetSelection(KSelection<KBuilder>* sel_) {
			sel = sel_;

			//check if initial reading of input has already been done
			if(sel->GetGlobalOpt()->Get("prepared_RA2bin",false)){
				//if the initial reading failed, abort
				if(sel->GetGlobalOpt()->Get("failed_RA2bin",false)){
					depfailed = true;
					return;
				}
				
				//otherwise grab assembled member vars
				sel->GetGlobalOpt()->Get("IDtoBinNumber",IDtoBinNumber);
				sel->GetGlobalOpt()->Get("RA2VarNames",RA2VarNames);
				sel->GetGlobalOpt()->Get("RA2VarMin",RA2VarMin);
				sel->GetGlobalOpt()->Get("RA2VarMax",RA2VarMax);
			}
			else { //assemble member vars from user input
				sel->GetGlobalOpt()->Get("RA2VarNames",RA2VarNames);
				
				vector<vector<unsigned> > all_bins;
				for(unsigned q = 0; q < RA2VarNames.size(); ++q){
					stringstream pre;
					pre << "RA2Var" << q;
					
					vector<float> min, max;
					sel->GetGlobalOpt()->Get(pre.str()+"Min",min);
					sel->GetGlobalOpt()->Get(pre.str()+"Max",max);
					if(min.size() != max.size()){
						cout << "Input error: vector length mismatches in " << pre.str() << " min and max specification. RA2 binning will not be computed." << endl;
						depfailed = true;
						sel->GetGlobalOpt()->Set<bool>("prepared_RA2bin",true);
						sel->GetGlobalOpt()->Set<bool>("failed_RA2bin",true);
						return;
					}
					
					vector<unsigned> bins;
					sel->GetGlobalOpt()->Get(pre.str()+"Bins",bins);
					if(q>0 && bins.size()!=all_bins[0].size()){
						cout << "Input error: vector length mismatches in " << pre.str() << " bins specification. RA2 binning will not be computed." << endl;
						depfailed = true;
						sel->GetGlobalOpt()->Set<bool>("prepared_RA2bin",true);
						sel->GetGlobalOpt()->Set<bool>("failed_RA2bin",true);
						return;	
					}
					
					//store member vars
					RA2VarMin.push_back(min);
					RA2VarMax.push_back(max);
					all_bins.push_back(bins);
				}
				
				//create map of RA2 bin IDs to bin numbers
				//and associated bin labels (in case they are requested)
				vector<string> labels;
				labels.reserve(all_bins[0].size());
				for(unsigned b = 0; b < all_bins[0].size(); ++b){
					vector<unsigned> bin_id;
					bin_id.reserve(all_bins.size());
					stringstream label;
					for(unsigned q = 0; q < all_bins.size(); ++q){
						bin_id.push_back(all_bins[q][b]);
						label << RA2VarNames[q] << all_bins[q][b]; //the specific bin label: currently just the bin number
						if(q<all_bins.size()-1) label << "_";
					}
					IDtoBinNumber[bin_id] = b+1; //bin numbers start at 1
					labels.push_back(label.str());
				}
				
				//store assembled member vars with global options
				sel->GetGlobalOpt()->Set<map<vector<unsigned>,unsigned> >("IDtoBinNumber",IDtoBinNumber);
				sel->GetGlobalOpt()->Set<vector<vector<float> > >("RA2VarMin",RA2VarMin);
				sel->GetGlobalOpt()->Set<vector<vector<float> > >("RA2VarMax",RA2VarMax);
				sel->GetGlobalOpt()->Set<vector<string> >("RA2bin_labels",labels);
			}
			
			//check other options
			RA2Exclusive = sel->GetGlobalOpt()->Get("RA2Exclusive",true);
			DoBTagSF = sel->GetGlobalOpt()->Get("btagcorr",false);
		}
		
		//used for non-dummy selectors
		virtual bool Cut() {
			if(depfailed) return false;
			
			RA2bins = GetBinNumbers(RA2binVec);
			
			return RA2bins.size()!=0;
		}
		
		//functions
		vector<unsigned> GetBinNumbers(vector<vector<unsigned> >& bin_vec) {
			vector<vector<unsigned> > bins;
			bins.reserve(RA2VarNames.size());
			for(int q = 0; q < RA2VarNames.size(); ++q){
				bins.push_back(GetBins(q));
				
				//skip loop if no bin was found for a value
				if(bins.back().size()==0) return vector<unsigned>();
			}
			
			//set up for variable # of for loops
			vector<unsigned> indices(RA2VarNames.size(),0);
			vector<unsigned> bin_num(RA2VarNames.size(),0);
			vector<unsigned> found_bins;
			vector<vector<unsigned> > found_bin_nums;
			FindBin(indices,bins,0,bin_num,found_bins,found_bin_nums);
			bin_vec = found_bin_nums;
			return found_bins;
		}
		//recursive function to implement variable # of for loops
		//ref: http://stackoverflow.com/questions/9555864/variable-nested-for-loops
		void FindBin(vector<unsigned>& indices, vector<vector<unsigned> >& bins, unsigned pos, vector<unsigned>& bin_num, vector<unsigned>& found_bins, vector<vector<unsigned> >& found_bin_nums){
			for(indices[pos] = 0; indices[pos] < bins[pos].size(); indices[pos]++){
				bin_num[pos] = bins[pos][indices[pos]];
				if(pos == indices.size()-1){
					map<vector<unsigned>, unsigned>::iterator it = IDtoBinNumber.find(bin_num);
					if(it != IDtoBinNumber.end()){
						found_bins.push_back(it->second);
						found_bin_nums.push_back(bin_num);
					}
				}
				else {
					FindBin(indices,bins,pos+1,bin_num,found_bins,found_bin_nums);
				}
			}
		}
		
		vector<unsigned> GetBins(unsigned qty){
			//assume all values are floats
			vector<float> val;
			if(RA2VarNames[qty]=="NJets") val.push_back(looper->NJets);
			else if(RA2VarNames[qty]=="BTags") {
				if(DoBTagSF){
					//put this event into all btag bins
					for(unsigned b = 0; b < RA2VarMax[qty].size(); ++b){
						val.push_back(RA2VarMax[qty][b]);
					}
				}
				else val.push_back(looper->BTags);
			}
			else if(RA2VarNames[qty]=="MHT") val.push_back(looper->MHT);
			else if(RA2VarNames[qty]=="HT") val.push_back(looper->HT);
			//else if(RA2VarNames[qty]=="ak1p2Jets_sumJetMass") val.push_back(looper->ak1p2Jets_sumJetMass);
			else {}			
			
			vector<unsigned> bins;
			for(unsigned v = 0; v < val.size(); ++v){
				for(unsigned n = 0; n < RA2VarMin[qty].size(); ++n){
					if(val[v] > RA2VarMin[qty][n] && val[v] <= RA2VarMax[qty][n]) bins.push_back(n);
				}
			}
			return bins;
		}
		
		unsigned GetBin(string qty_name, const vector<unsigned>& bin_num){
			for(int q = 0; q < RA2VarNames.size(); ++q){
				if(RA2VarNames[q]==qty_name) return bin_num[q];
			}
			return -1;
		}
		
	public:
		//member variables
		bool RA2Exclusive, DoBTagSF;
		vector<unsigned> RA2bins;
		vector<vector<unsigned> > RA2binVec;
		map<vector<unsigned>, unsigned> IDtoBinNumber;
		vector<string> RA2VarNames;
		vector<vector<float> > RA2VarMin, RA2VarMax;
};

//---------------------------------------------------------------
//checks for double-counted events
class KDoubleCountSelector : public KSelector<KBuilder> {
	public:
		//constructor
		KDoubleCountSelector() : KSelector<KBuilder>() { }
		KDoubleCountSelector(string name_, OptionMap* localOpt_) : KSelector<KBuilder>(name_,localOpt_) { }
		
		//used for non-dummy selectors
		virtual bool Cut() {
			pair<int,int> tmp(looper->RunNum,looper->EvtNum);
			map<pair<int,int>,int>::iterator cmit = countmap.find(tmp);
			if(cmit!=countmap.end()) return false; //fails the event if it finds a double count
			else {
				countmap[tmp] = 1;
				return true;
			}
		}
		
		//member variables
		map<pair<int,int>,int> countmap;
};

//---------------------------------------------------------------
//applies photon ID to candidates
class KPhotonIDSelector : public KSelector<KBuilder> {
	public:
		//constructor
		KPhotonIDSelector() : KSelector<KBuilder>() { }
		KPhotonIDSelector(string name_, OptionMap* localOpt_) : KSelector<KBuilder>(name_,localOpt_) { 
			canfail = false;
			//check for option
			best = localOpt->Get("best",false);
		}
		
		//used for non-dummy selectors
		virtual bool Cut() {
			goodPhotons.clear();
			for(unsigned p = 0; p < looper->photonCands->size(); ++p){
				bool goodPhoton = abs(looper->photonCands->at(p).Eta())<1.4442 || ((abs(looper->photonCands->at(p).Eta())>1.566 && abs(looper->photonCands->at(p).Eta())<2.5))
									&& looper->photonCands->at(p).Pt()>100.
									&& ((looper->photon_hadTowOverEM->at(p)<0.028 && !looper->photon_hasPixelSeed->at(p) && looper->photon_isEB->at(p)) 
										|| (looper->photon_hadTowOverEM->at(p)<0.093 && !looper->photon_hasPixelSeed->at(p) && !looper->photon_isEB->at(p)))
									&& looper->photon_pfChargedIsoRhoCorr->at(p)<(2.67*looper->photon_isEB->at(p) + 1.79*!looper->photon_isEB->at(p))
									&& looper->photon_pfGammaIsoRhoCorr->at(p)<((2.11+0.0014*looper->photonCands->at(p).Pt())*looper->photon_isEB->at(p)
																					+ (3.09+0.0091*looper->photonCands->at(p).Pt())*!looper->photon_isEB->at(p))
									&& looper->photon_pfNeutralIsoRhoCorr->at(p)<((7.23+exp(0.0028*looper->photonCands->at(p).Pt()+0.5408))*looper->photon_isEB->at(p)
																					+ (8.89+0.01725*looper->photonCands->at(p).Pt())*!looper->photon_isEB->at(p));
				if(best) goodPhoton &= looper->bestPhoton->size()>0 && KMath::DeltaR(looper->bestPhoton->at(0).Phi(),looper->bestPhoton->at(0).Eta(),looper->photonCands->at(p).Phi(),looper->photonCands->at(p).Eta()) < 0.15;
				if(goodPhoton) goodPhotons.push_back(p);
			}
			return true;
		}
		
		//member variables
		bool best;
		vector<unsigned> goodPhotons;
};

//---------------------------------------------------------------
//applies MET filters
class KMETFilterSelector : public KSelector<KBuilder> {
	public:
		//constructor
		KMETFilterSelector() : KSelector<KBuilder>() { }
		KMETFilterSelector(string name_, OptionMap* localOpt_) : KSelector<KBuilder>(name_,localOpt_) { }

		//turn on necessary branches
		virtual void SetLooper(KBuilder* looper_) {
			looper = looper_;
			
			looper->fChain->SetBranchStatus("NVtx",1);
			looper->fChain->SetBranchStatus("eeBadScFilter",1);
			looper->fChain->SetBranchStatus("HBHENoiseFilter",1);
			looper->fChain->SetBranchStatus("JetID",1);
		}
		
		//used for non-dummy selectors
		virtual bool Cut() {
			return looper->NVtx > 0 && looper->eeBadScFilter && looper->HBHENoiseFilter && looper->JetID;
		}
};

//---------------------------------------------------------------
//calculates btag scale factors
class KBTagSFSelector : public KSelector<KBuilder> {
	public:
		//constructor
		KBTagSFSelector() : KSelector<KBuilder>() { }
		KBTagSFSelector(string name_, OptionMap* localOpt_) : KSelector<KBuilder>(name_,localOpt_), debug(false), btagSFunc(0), mistagSFunc(0), h_eff_b(NULL), h_eff_c(NULL), h_eff_udsg(NULL) { 
			canfail = false;
			
			//check for option
			debug = localOpt->Get("debug",false);
			
			//initialize btag helper classes
			calib = BTagCalibration("csvv1","btag/CSVSLV1.csv");
			reader = BTagCalibrationReader(&calib, BTagEntry::OP_MEDIUM, "comb", "central");
			readerUp = BTagCalibrationReader(&calib, BTagEntry::OP_MEDIUM, "comb", "up");
			readerDown = BTagCalibrationReader(&calib, BTagEntry::OP_MEDIUM, "comb", "down");
		}
		virtual void SetLooper(KBuilder* looper_) {
			looper = looper_;
			
			//check for option
			looper->globalOpt->Get("btagSFunc",btagSFunc);
			looper->globalOpt->Get("mistagSFunc",mistagSFunc);

			//get efficiency histograms
			TFile* file = looper->MyBase->GetFile();
			h_eff_b = (TH2F*)file->Get("h_eff_b");
			h_eff_c = (TH2F*)file->Get("h_eff_c");
			h_eff_udsg = (TH2F*)file->Get("h_eff_udsg");
			
			if(!h_eff_b || !h_eff_c || !h_eff_udsg){
				cout << "Input error: b-tag efficiency histograms missing!" << endl;
				depfailed = true;
			}
		}
		
		//used for non-dummy selectors
		virtual bool Cut() {
			//reset probabilities
			prob = vector<double>(4,0.0);
			prob[0] = 1.0;
			
			//first loop over jets
			vector<vector<double> > sfEffLists = vector<vector<double> >(looper->Jets->size(),vector<double>());
			for(unsigned ja = 0; ja < looper->Jets->size(); ++ja){
				//HT jet cuts
				if(looper->Jets->at(ja).Pt() <= 30 || fabs(looper->Jets->at(ja).Eta()) >= 2.4) continue;
				
				//get sf and eff values (checks if already calculated)
				InitSFEff(looper->Jets->at(ja).Pt(), looper->Jets->at(ja).Eta(), looper->Jets_flavor->at(ja), sfEffLists[ja]);
				double eps_a = sfEffLists[ja][0]*sfEffLists[ja][1];
				
				//jet index, pt, eta, flavor, eff, sf
				if(debug) cout << "Jet " << ja << ": " << looper->Jets->at(ja).Pt() << ", " << fabs(looper->Jets->at(ja).Eta()) << ", " << abs(looper->Jets_flavor->at(ja)) 
								<< ", " << sfEffLists[ja][0] << ", " << sfEffLists[ja][1] << endl;
				
				//calculate prob(0 b-tags)
				prob[0] *= (1-eps_a);
				
				//sub-probabilities for following calculations
				double subprob1 = 1.0;
				double subprob2 = 0.0;
				
				//second loop over jets
				for(unsigned jb = 0; jb < looper->Jets->size(); ++jb){
					//skip the same jet
					if(jb==ja) continue;
					
					//HT jet cuts
					if(looper->Jets->at(jb).Pt() <= 30 || fabs(looper->Jets->at(jb).Eta()) >= 2.4) continue;
					
					//get sf and eff values (checks if already calculated)
					InitSFEff(looper->Jets->at(jb).Pt(), looper->Jets->at(jb).Eta(), looper->Jets_flavor->at(jb), sfEffLists[jb]);
					double eps_b = sfEffLists[jb][0]*sfEffLists[jb][1];
					
					//jet index, pt, eta, flavor, eff, sf
					if(debug) cout << "\tJet " << jb << ": " << looper->Jets->at(jb).Pt() << ", " << fabs(looper->Jets->at(jb).Eta()) << ", " << abs(looper->Jets_flavor->at(jb)) 
									<< ", " << sfEffLists[jb][0] << ", " << sfEffLists[jb][1] << endl;
					
					//calculate prob(1 b-tag)
					subprob1 *= (1-eps_b);
					
					//sub-sub-probability for following calculations
					double subsubprob2 = 1.0;
					
					//third loop over jets (only for jb>ja)
					if(jb<ja) continue;
					for(unsigned jc = 0; jc < looper->Jets->size(); ++jc){
						//skip the same jet
						if(jc==jb || jc==ja) continue;
						
						//HT jet cuts
						if(looper->Jets->at(jc).Pt() <= 30 || fabs(looper->Jets->at(jc).Eta()) >= 2.4) continue;
						
						//get sf and eff values (checks if already calculated)
						InitSFEff(looper->Jets->at(jc).Pt(), looper->Jets->at(jc).Eta(), looper->Jets_flavor->at(jc), sfEffLists[jc]);
						double eps_c = sfEffLists[jc][0]*sfEffLists[jc][1];
						
						//jet index, pt, eta, flavor, eff, sf
						if(debug) cout << "\t\tJet " << jc << ": " << looper->Jets->at(jc).Pt() << ", " << fabs(looper->Jets->at(jc).Eta()) << ", " << abs(looper->Jets_flavor->at(jc)) 
										<< ", " << sfEffLists[jc][0] << ", " << sfEffLists[jc][1] << endl;
						
						//calculate prob(2 b-tags)
						subsubprob2 *= (1-eps_c);
					}
					
					//add up sub-sub-prob
					subprob2 += eps_b*subsubprob2;
				}
				
				//add up sub-probs
				prob[1] += eps_a*subprob1;
				prob[2] += eps_a*subprob2;
			}
			
			//conserve probability
			prob[3] = 1 - prob[0] - prob[1] - prob[2];
			if(debug) cout << prob[0] << ", " << prob[1] << ", " << prob[2] << ", " << prob[3] << endl;
			
			return true;
		}
		
		//helper functions
		void InitSFEff(double pt, double eta, int flav, vector<double>& sfEffList){
			//avoid rerunning this
			if(sfEffList.size()>0) return;
			
			//use abs(eta) for now
			eta = fabs(eta);
			//use abs(flav) always
			flav = abs(flav);
			
			sfEffList = vector<double>(2,1.0); //eff, sf (central, up, or down)
			
			if(flav==5){
				sfEffList[0] = h_eff_b->GetBinContent(h_eff_b->FindBin(pt,eta));
				sfEffList[1] = (btagSFunc==0 ? reader.eval(BTagEntry::FLAV_B,eta,pt) :
							   (btagSFunc==1 ? readerUp.eval(BTagEntry::FLAV_B,eta,pt) :
							               readerDown.eval(BTagEntry::FLAV_B,eta,pt) ) );
			}
			else if(flav==4){ //charm mistag unc taken to be 2x b-tag unc
				sfEffList[0] = h_eff_c->GetBinContent(h_eff_c->FindBin(pt,eta));
				double sf = reader.eval(BTagEntry::FLAV_B,eta,pt);
				sfEffList[1] = (btagSFunc==0 ? sf :
							   (btagSFunc==1 ? 2*readerUp.eval(BTagEntry::FLAV_B,eta,pt) - sf :
							               2*readerDown.eval(BTagEntry::FLAV_B,eta,pt) - sf ) );
			}
			else if(flav<4 || flav==21){
				sfEffList[0] = h_eff_udsg->GetBinContent(h_eff_udsg->FindBin(pt,eta));
				sfEffList[1] = (mistagSFunc==0 ? reader.eval(BTagEntry::FLAV_UDSG,eta,pt) :
							   (mistagSFunc==1 ? readerUp.eval(BTagEntry::FLAV_UDSG,eta,pt) :
							               readerDown.eval(BTagEntry::FLAV_UDSG,eta,pt) ) );
			}
		}
		
		//member variables
		bool debug;
		int btagSFunc, mistagSFunc;
		BTagCalibration calib;
		BTagCalibrationReader reader, readerUp, readerDown;
		TH2F *h_eff_b, *h_eff_c, *h_eff_udsg;
		vector<double> prob;
};

//---------------------------------------------------------------
//little class to store value & weight pairs for filling histos
class KValue {
	public:
		//constructor
		KValue() : values(0), weights(0) {}
		//accessors
		void Fill(double v, double w=1){
			values.push_back(v);
			weights.push_back(w);
		}
		double & GetValue(int iv) { return values[iv]; }
		double & GetWeight(int iw) { return weights[iw]; }
		int GetSize() { return values.size(); }
		
	protected:
		//member variables
		vector<double> values;
		vector<double> weights;
	
};

//----------------------------------------------------
//final selector to fill histograms
//(accounts for dependence on other selectors)
class KHistoSelector : public KSelector<KBuilder> {
	public:
		//constructor
		KHistoSelector() : KSelector<KBuilder>() { }
		KHistoSelector(string name_, OptionMap* localOpt_) : KSelector<KBuilder>(name_,localOpt_), RA2Bin(NULL), PhotonID(NULL), BTagSF(NULL) { 
			canfail = false;
		}
		
		virtual void SetSelection(KSelection<KBuilder>* sel_) {
			sel = sel_;
			//set dependencies here
			RA2Bin = sel->Get<KRA2BinSelector*>("RA2Bin");
			PhotonID = sel->Get<KPhotonIDSelector*>("PhotonID");
			bool DoBTagSF = sel->GetGlobalOpt()->Get("btagcorr",false);
			if(DoBTagSF) BTagSF = sel->Get<KBTagSFSelector*>("BTagSF");
		} 
		
		//used for non-dummy selectors
		virtual bool Cut() {
			double w = looper->GetWeight();
			
			for(int h = 0; h < looper->htmp.size(); h++){
				unsigned vsize = looper->vars[h].size();
				vector<KValue> values(vsize);				
			
				for(int i = 0; i < vsize; i++){
					string vname = looper->vars[h][i];
					//list of cases for histo calculation and filling
					if(vname=="RA2bin" && RA2Bin){ //plot yield vs. bin of RA2 search -> depends on RA2Bin selector
						if(RA2Bin->RA2Exclusive) values[i].Fill(RA2Bin->RA2bins[0],w);
						else {
							for(int b = 0; b < RA2Bin->RA2bins.size(); b++){
								double wb = w;
								//weight by btag scale factor probability if available
								if(BTagSF) {
									int nb = RA2Bin->GetBin("BTags",RA2Bin->RA2binVec[b]);
									if(nb>=0 && nb<BTagSF->prob.size()) wb *= BTagSF->prob[nb];
									else wb = 0; //btag sf failed
								}
								values[i].Fill(RA2Bin->RA2bins[b],wb);
							}
						}
					}
					else if(vname=="njets"){//jet multiplicity
						values[i].Fill(looper->NJets,w);
					}
					else if(vname=="nbjets"){//b-jet multiplicity
						values[i].Fill(looper->BTags,w);
					}
					else if(vname=="ht"){//sum of jet pt
						values[i].Fill(looper->HT,w);
					}
					else if(vname=="mht"){//missing hadronic energy
						values[i].Fill(looper->MHT,w);
					}
					else if(vname=="mindeltaphiN"){//min normalized deltaphi between jets and MET
						values[i].Fill(looper->minDeltaPhiN,w);
					}
					else if(vname=="nleptons"){//# leptons (mu or ele)
						values[i].Fill(looper->Leptons,w);
					}
					else if(vname=="nelectrons"){//# electrons
						values[i].Fill(looper->Electrons->size(),w);
					}
					else if(vname=="nmuons"){//# muons
						values[i].Fill(looper->Muons->size(),w);
					}
					else if(vname=="nisotrack"){//# iso tracks
						values[i].Fill(looper->isoElectronTracks+looper->isoMuonTracks+looper->isoPionTracks,w);
					}
					else if(vname=="nvertex"){//# good vertices
						values[i].Fill(looper->NVtx,w);
					}
					else if(vname=="genht"){//# gen HT
						values[i].Fill(looper->genHT,w);
					}
					else if(vname=="sigmaietaieta"){//sigma ieta ieta variable for all photon candidates
						if(PhotonID){
							for(unsigned p = 0; p < PhotonID->goodPhotons.size(); ++p){
								values[i].Fill(looper->photon_sigmaIetaIeta->at(PhotonID->goodPhotons[p]),w);
							}
						}
						else { //if no ID applied, just plot everything
							for(unsigned p = 0; p < looper->photon_sigmaIetaIeta->size(); ++p){
								values[i].Fill(looper->photon_sigmaIetaIeta->at(p),w);
							}
						}
					}
					else if(vname=="bestsigmaietaieta"){//sigma ieta ieta variable for best photon
						if(looper->bestPhoton->size()==0) continue;
						int bestIndex = -1;
						for(unsigned p = 0; p < looper->photonCands->size(); ++p){
							if(looper->bestPhoton->at(0)==looper->photonCands->at(p) && looper->photon_isEB->at(p)){
								values[i].Fill(looper->photon_sigmaIetaIeta->at(p),w);
								break;
							}
						}
					}
					else { //if it's a histogram with no known variable or calculation, do nothing
					}
				}
				
				//now fill the histogram
				TH1* htmp = looper->htmp[h];
				if(vsize==1){
					for(int ix = 0; ix < values[0].GetSize(); ix++){
						htmp->Fill(values[0].GetValue(ix), values[0].GetWeight(ix));
					}
				}
				else if(vsize==2){
					//need to cast in order to use Fill(x,y,w)
					//these three cases allow for various x vs. y comparisons: same # entries per event, or 1 vs. N per event
					if(values[0].GetSize()==values[1].GetSize()) {
						for(int i = 0; i < values[0].GetSize(); i++){
							if(htmp->GetDimension()==1)
								static_cast<TProfile*>(htmp)->Fill(values[0].GetValue(i), values[1].GetValue(i), values[0].GetWeight(i)); //pick the x weight by default
							else if(htmp->GetDimension()==2)
								static_cast<TH2*>(htmp)->Fill(values[0].GetValue(i), values[1].GetValue(i), values[0].GetWeight(i)); //pick the x weight by default
						}
					}
					else if(values[0].GetSize()==1){
						for(int iy = 0; iy < values[1].GetSize(); iy++){
							if(htmp->GetDimension()==1)
								static_cast<TProfile*>(htmp)->Fill(values[0].GetValue(0), values[1].GetValue(iy), values[1].GetWeight(iy));
							else if(htmp->GetDimension()==2)
								static_cast<TH2*>(htmp)->Fill(values[0].GetValue(0), values[1].GetValue(iy), values[1].GetWeight(iy));
						}
					}
					else if(values[1].GetSize()==1){
						for(int ix = 0; ix < values[0].GetSize(); ix++){
							if(htmp->GetDimension()==1)
								static_cast<TProfile*>(htmp)->Fill(values[0].GetValue(ix), values[1].GetValue(0), values[0].GetWeight(ix));
							else if(htmp->GetDimension()==2)
								static_cast<TH2*>(htmp)->Fill(values[0].GetValue(ix), values[1].GetValue(0), values[0].GetWeight(ix));
						}
					}
				}
				else { //no support for other # of vars
				}
			}
			
			return true;
		}
		
		//member variables
		KRA2BinSelector* RA2Bin;
		KPhotonIDSelector* PhotonID;
		KBTagSFSelector* BTagSF;
};

//-------------------------------------------------------------
//addition to KParser to create selectors
namespace KParser {
	template <>
	KSelector<KBuilder>* processSelector<KBuilder>(KNamed* tmp){
		KSelector<KBuilder>* srtmp = 0;
		string sname = tmp->first;
		OptionMap* omap = tmp->second;
		
		//check for all known selectors
		if(sname=="Histo") srtmp = new KHistoSelector(sname,omap);
		else if(sname=="DoubleCount") srtmp = new KDoubleCountSelector(sname,omap);
		else if(sname=="RA2Bin") srtmp = new KRA2BinSelector(sname,omap);
		else if(sname=="PhotonID") srtmp = new KPhotonIDSelector(sname,omap);
		else if(sname=="BTagSF") srtmp = new KBTagSFSelector(sname,omap);
		else if(sname=="METFilter") srtmp = new KMETFilterSelector(sname,omap);
		else {} //skip unknown selectors
		
		if(!srtmp) cout << "Input error: unknown selector " << sname << ". This selector will be skipped." << endl;
		
		return srtmp;
	}
}

#endif