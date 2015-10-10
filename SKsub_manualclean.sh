#!/bin/bash

KEEPTAR=$1
JOBDIR=batch
STORE=root://cmseos.fnal.gov//store/user/pedrok/SUSY2015/Analysis/Skims/Run2ProductionV2

./SKcheck.sh "$KEEPTAR" "$JOBDIR"

SAMPLES=(
GJets_HT-100to200 \
GJets_HT-200to400 \
GJets_HT-400to600 \
GJets_HT-600toInf \
QCD_HT-200to300 \
QCD_HT-300to500 \
QCD_HT-500to700 \
QCD_HT-700to1000 \
QCD_HT-1000to1500 \
QCD_HT-1500to2000 \
QCD_HT-2000toInf \
SinglePhoton_2015C \
SinglePhoton_2015D
)

for SAMPLE in ${SAMPLES[@]}
  do
    ./SKtemp.sh ${JOBDIR} input/input_selection.txt ${SAMPLE} GJet_ManualCleanVars root://cmseos.fnal.gov//store/user/lpcsusyhad/SusyRA2Analysis2015/Run2ProductionV2 tree ${STORE}
  done