//
// File: PhylogeneticsApplicationTools.cpp
// Created by: Julien Dutheil
// Created on: Fri Oct 21 16:49 2005
// from old file ApplicationTools.cpp created on Sun Dec 14 09:36:26 2003
//

/*
Copyright or � or Copr. CNRS, (November 16, 2004)

This software is a computer program whose purpose is to provide classes
for phylogenetic data analysis.

This software is governed by the CeCILL  license under French law and
abiding by the rules of distribution of free software.  You can  use, 
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info". 

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability. 

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or 
data to be ensured and,  more generally, to use and operate it in the 
same conditions as regards security. 

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.
*/

#include "PhylogeneticsApplicationTools.h"
#include "models"
#include "OptimizationTools.h"
#include "Tree.h"
#include "Newick.h"
#include "HomogeneousTreeLikelihood.h"

// From Utils:
#include <Utils/FileTools.h>
#include <Utils/TextTools.h>
#include <Utils/ApplicationTools.h>
#include <Utils/StringTokenizer.h>

// From NumCalc:
#include <NumCalc/ConstantDistribution.h>
#include <NumCalc/GammaDiscreteDistribution.h>

// From the STL:
#include <fstream>
#include <iomanip>
using namespace std;

/******************************************************************************/

TreeTemplate<Node> * PhylogeneticsApplicationTools::getTree(
	map<string, string> & params,
	const string & suffix,
	bool suffixIsOptional,
	bool verbose)
{
	string treeFilePath = ApplicationTools::getAFilePath("tree.file", params, true, true, suffix, suffixIsOptional);
	
	//Read the tree file:
	Newick newick(true);
	TreeTemplate<Node> * tree = dynamic_cast<TreeTemplate<Node> *>(newick.read(treeFilePath));
	if(verbose) ApplicationTools::displayResult("Tree file", treeFilePath);
	return tree;
}

/******************************************************************************/

SubstitutionModel * PhylogeneticsApplicationTools::getSubstitutionModel(
	const Alphabet * alphabet,
	const SiteContainer * data,
	map<string, string> & params,
	const string & suffix,
	bool suffixIsOptional,
	bool verbose)
{
	string modelName = ApplicationTools::getStringParameter("model", params, "JCnuc", suffix, suffixIsOptional);
	SubstitutionModel * model = NULL;

	if(alphabet -> getAlphabetType() == "DNA alphabet"
	|| alphabet -> getAlphabetType() == "RNA alphabet") {

		const NucleicAlphabet * alpha = dynamic_cast<const NucleicAlphabet *>(alphabet);

		if(modelName == "GTR") {
			double piA = 0.25, piC = 0.25, piG = 0.25, piT = 0.25;
			double a = ApplicationTools::getDoubleParameter("a", params, 1, suffix, suffixIsOptional);
			double b = ApplicationTools::getDoubleParameter("b", params, 1, suffix, suffixIsOptional);
			double c = ApplicationTools::getDoubleParameter("c", params, 1, suffix, suffixIsOptional);
			double d = ApplicationTools::getDoubleParameter("d", params, 1, suffix, suffixIsOptional);
			double e = ApplicationTools::getDoubleParameter("e", params, 1, suffix, suffixIsOptional);
			double f = ApplicationTools::getDoubleParameter("f", params, 1, suffix, suffixIsOptional);
			bool useObsFreq = ApplicationTools::getBooleanParameter("model.use_observed_freq", params, false, suffix, suffixIsOptional);
			if(useObsFreq && data != NULL) {
				model = new GTR(alpha, a, b, c, d, e, f);
				dynamic_cast<TN93 *>(model) -> setFreqFromData(*data);
				piA = model -> getParameterValue("piA");
				piC = model -> getParameterValue("piC");
				piG = model -> getParameterValue("piG");
				piT = model -> getParameterValue("piT");
			} else {
				piA = ApplicationTools::getDoubleParameter("piA", params, 0.25, suffix, suffixIsOptional);
				piC = ApplicationTools::getDoubleParameter("piC", params, 0.25, suffix, suffixIsOptional);
				piG = ApplicationTools::getDoubleParameter("piG", params, 0.25, suffix, suffixIsOptional);
				piT = ApplicationTools::getDoubleParameter("piT", params, 0.25, suffix, suffixIsOptional);
				if( fabs(1-(piA + piT + piG + piC)) > 0.00000000000001 ) {
					ApplicationTools::displayError("Equilibrium base frequencies must equal 1. Aborting...");
					exit(-1);
				}
				model = new GTR(alpha, a, b, c, d, e, f, piA, piC, piG, piT);
			}
			if(verbose) {
				ApplicationTools::displayResult("model" , modelName);
				ApplicationTools::displayResult("a", TextTools::toString(a));
				ApplicationTools::displayResult("b", TextTools::toString(b));
				ApplicationTools::displayResult("c", TextTools::toString(c));
				ApplicationTools::displayResult("d", TextTools::toString(d));
				ApplicationTools::displayResult("e", TextTools::toString(e));
				ApplicationTools::displayResult("f", TextTools::toString(f));
				ApplicationTools::displayResult("piA"   , TextTools::toString(piA));
				ApplicationTools::displayResult("piC"   , TextTools::toString(piC));
				ApplicationTools::displayResult("piG"   , TextTools::toString(piG));
				ApplicationTools::displayResult("piT"   , TextTools::toString(piT));
			}
		} else if(modelName == "TN93") {
			double piA = 0.25, piC = 0.25, piG = 0.25, piT = 0.25;
			double kappa1 = ApplicationTools::getDoubleParameter("kappa1", params, 2, suffix, suffixIsOptional);
			double kappa2 = ApplicationTools::getDoubleParameter("kappa2", params, 2, suffix, suffixIsOptional);
			bool useObsFreq = ApplicationTools::getBooleanParameter("model.use_observed_freq", params, false, suffix, suffixIsOptional);
			if(useObsFreq && data != NULL) {
				model = new TN93(alpha, kappa1, kappa2);
				dynamic_cast<TN93 *>(model) -> setFreqFromData(*data);
				piA = model -> getParameterValue("piA");
				piC = model -> getParameterValue("piC");
				piG = model -> getParameterValue("piG");
				piT = model -> getParameterValue("piT");
			} else {
				piA = ApplicationTools::getDoubleParameter("piA", params, 0.25, suffix, suffixIsOptional);
				piC = ApplicationTools::getDoubleParameter("piC", params, 0.25, suffix, suffixIsOptional);
				piG = ApplicationTools::getDoubleParameter("piG", params, 0.25, suffix, suffixIsOptional);
				piT = ApplicationTools::getDoubleParameter("piT", params, 0.25, suffix, suffixIsOptional);
				if( fabs(1-(piA + piT + piG + piC)) > 0.00000000000001 ) {
					ApplicationTools::displayError("Equilibrium base frequencies must equal 1. Aborting...");
					exit(-1);
				}
				model = new TN93(alpha, kappa1, kappa2, piA, piC, piG, piT);
			}
			if(verbose) {
				ApplicationTools::displayResult("model" , modelName);
				ApplicationTools::displayResult("kappa1", TextTools::toString(kappa1));
				ApplicationTools::displayResult("kappa2", TextTools::toString(kappa2));
				ApplicationTools::displayResult("piA"   , TextTools::toString(piA));
				ApplicationTools::displayResult("piC"   , TextTools::toString(piC));
				ApplicationTools::displayResult("piG"   , TextTools::toString(piG));
				ApplicationTools::displayResult("piT"   , TextTools::toString(piT));
			}
		} else if(modelName == "HKY85") {
			double piA = 0.25, piC = 0.25, piG = 0.25, piT = 0.25;
			double kappa = ApplicationTools::getDoubleParameter("kappa", params, 2, suffix, suffixIsOptional);
			bool useObsFreq = ApplicationTools::getBooleanParameter("model.use_observed_freq", params, false, suffix, suffixIsOptional);
			if(useObsFreq && data != NULL) {
				model = new HKY85(alpha, kappa);
				dynamic_cast<HKY85 *>(model) -> setFreqFromData(*data);
				piA = model -> getParameterValue("piA");
				piC = model -> getParameterValue("piC");
				piG = model -> getParameterValue("piG");
				piT = model -> getParameterValue("piT");
			} else {
				piA = ApplicationTools::getDoubleParameter("piA", params, 0.25, suffix, suffixIsOptional);
				piC = ApplicationTools::getDoubleParameter("piC", params, 0.25, suffix, suffixIsOptional);
				piG = ApplicationTools::getDoubleParameter("piG", params, 0.25, suffix, suffixIsOptional);
				piT = ApplicationTools::getDoubleParameter("piT", params, 0.25, suffix, suffixIsOptional);
				if( fabs(1-(piA + piT + piG + piC)) > 0.00000000000001 ) {
					ApplicationTools::displayError("Equilibrium base frequencies must equal 1. Aborting...");
					exit(-1);
				}
				model = new HKY85(alpha, kappa, piA, piC, piG, piT);
			}
			if(verbose) {
				ApplicationTools::displayResult("model", modelName);
				ApplicationTools::displayResult("kappa", TextTools::toString(kappa));
				ApplicationTools::displayResult("piA"  , TextTools::toString(piA));
				ApplicationTools::displayResult("piC"  , TextTools::toString(piC));
				ApplicationTools::displayResult("piG"  , TextTools::toString(piG));
				ApplicationTools::displayResult("piT"  , TextTools::toString(piT));
			}
		} else if(modelName == "T92") {
			double kappa = ApplicationTools::getDoubleParameter("kappa", params, 2, suffix, suffixIsOptional);
			double theta = 0.5;
			bool useObsFreq = ApplicationTools::getBooleanParameter("model.use_observed_freq", params, false, suffix, suffixIsOptional);
			if(useObsFreq && data != NULL) {
				model = new T92(alpha, kappa);
				dynamic_cast<T92 *>(model) -> setFreqFromData(*data);
				theta = model -> getParameterValue("theta");
			} else {
				theta = ApplicationTools::getDoubleParameter("theta", params, 0.5, suffix, suffixIsOptional);
				model = new T92(alpha, kappa, theta);
			}
			if(verbose) {
				ApplicationTools::displayResult("model", modelName);
				ApplicationTools::displayResult("kappa", TextTools::toString(kappa));
				ApplicationTools::displayResult("theta", TextTools::toString(theta));
			}
		} else if(modelName == "K80") {
    	double kappa = ApplicationTools::getDoubleParameter("kappa", params, 2, suffix, suffixIsOptional);
			model = new K80(alpha, kappa);
  		if(verbose) {
				ApplicationTools::displayResult("model", modelName);
				ApplicationTools::displayResult("kappa", TextTools::toString(kappa));
			}
  	} else if(modelName == "JCnuc") {
			model = new JCnuc(alpha);
  		if(verbose) {
				ApplicationTools::displayResult("model", modelName);
			}
		} else {
			ApplicationTools::displayError("Model '" + modelName + "' unknown. Aborting..."); //It would be better to throw an exception!
			exit(-1);
		}
	} else { // Alphabet supposed to be proteic!
		const ProteicAlphabet * alpha = dynamic_cast<const ProteicAlphabet *>(alphabet);
		bool useObsFreq = ApplicationTools::getBooleanParameter("model.use_observed_freq", params, false, suffix, suffixIsOptional);
		if(modelName == "JCprot") {
			model = new JCprot(alpha);
		}	else if(modelName == "DSO78") {
			model = new DSO78(alpha);
			if(useObsFreq && data != NULL) dynamic_cast<DSO78 *>(model) -> setFreqFromData(*data);
		} else if(modelName == "JTT92") {
			model = new JTT92(alpha);
			if(useObsFreq && data != NULL) dynamic_cast<JTT92 *>(model) -> setFreqFromData(*data);	
		} else if(modelName == "empirical") {
			string file = ApplicationTools::getAFilePath("model_empirical.file", params, true, true, suffix, true);
			model = new UserProteinSubstitutionModel(alpha, file);
			if(useObsFreq && data != NULL) dynamic_cast<UserProteinSubstitutionModel *>(model) -> setFreqFromData(*data);	
			} else {
			ApplicationTools::displayError("Model '" + modelName + "' unknown. Aborting...");
		}
 		if(verbose) {
			ApplicationTools::displayResult("model", modelName + (useObsFreq && (model != NULL) ? "-F" : ""));
		}
	}
	return model;
}

/******************************************************************************/

void PhylogeneticsApplicationTools::printSubstitutionModelHelp()
{
	ApplicationTools::message << "Substitution Model:" << endl;
	ApplicationTools::message << "model               | Nucleotides (N): [JCnuc, K80, T92, HKY85, TN93]" << endl;
  ApplicationTools::message << "                    | Proteins (P): [JCprot, DSO78, JTT92, empirical]" << endl;
  ApplicationTools::message << "kappa               | kappa(N)  parameter in Q matrix" << endl;
  ApplicationTools::message << "kappa1              | kappa1(N) parameter in Q matrix" << endl;
  ApplicationTools::message << "kappa2              | kappa2(N) parameter in Q matrix" << endl;
  ApplicationTools::message << "a,b,c,d,e,f         | GTR rates parameter in Q matrix" << endl;
  ApplicationTools::message << "theta               | theta(N)  parameter in Q matrix" << endl;
  ApplicationTools::message << "piA                 | piA(N)    parameter in Q matrix" << endl;
  ApplicationTools::message << "piT                 | piT(N)    parameter in Q matrix" << endl;
  ApplicationTools::message << "piC                 | piC(N)    parameter in Q matrix" << endl;
  ApplicationTools::message << "piG                 | piG(N)    parameter in Q matrix" << endl;
	ApplicationTools::message << "use_observed_freq   | (N,P) Tell if the observed frequencies must be used." << endl; 
	ApplicationTools::message << "model_empirical.file| (P) The path toward data file to use (PAML format)." << endl; 
}

/******************************************************************************/

DiscreteDistribution * PhylogeneticsApplicationTools::getRateDistribution(
	map<string, string> & params,
	string suffix,
	bool suffixIsOptional,
	bool verbose
)
{
	string distributionType = ApplicationTools::getStringParameter("rate_distribution", params, "constant", suffix, suffixIsOptional);
	DiscreteDistribution * rDist;
	if(distributionType == "constant") {
		rDist = new ConstantDistribution(1.);
		if(verbose) {
			ApplicationTools::displayResult("rate_distribution", distributionType);
		}
	} else if(distributionType == "gamma") {
		double alpha = ApplicationTools::getDoubleParameter("rate_distribution_gamma.alpha", params, 1., suffix, suffixIsOptional);
		int nbClasses = ApplicationTools::getIntParameter("rate_distribution.classes_number", params, 4, suffix, suffixIsOptional);

		if(alpha < 0) {
			ApplicationTools::displayError("Alpha parameter in gamma distribution of rates must be > 0, found " + TextTools::toString(alpha) + ".");
			exit(-1);
		} else {
			rDist = new GammaDiscreteDistribution(nbClasses, alpha);
			if(verbose) {
				ApplicationTools::displayResult("Rate distribution", distributionType);
				ApplicationTools::displayResult("shape", TextTools::toString((dynamic_cast<GammaDiscreteDistribution *>(rDist)) -> getParameterValue("alpha")));
				ApplicationTools::displayResult("# classes", TextTools::toString(rDist -> getNumberOfCategories()));
				for(unsigned int c = 0; c < rDist -> getNumberOfCategories(); c++) {
					ApplicationTools::displayResult("* Category " + TextTools::toString(c)
					+ "(rate = " + TextTools::toString(rDist -> getCategory(c))
					+ "), prob = ", TextTools::toString(rDist -> getProbability(c)));
				}
			}
		}
	} else {
		ApplicationTools::displayError("Distribution unknown: " + distributionType + ".");
		exit(-1);
	}

	return rDist;
}

/******************************************************************************/

void PhylogeneticsApplicationTools::printRateDistributionHelp()
{
 	ApplicationTools::message << "rate_distribution   | uniform or gamma." << endl;
	ApplicationTools::message << "shape               | the gamma law's alpha parameter." << endl;
	ApplicationTools::message << "classes_number      | discrete approximation: number of categories (default to 4)." << endl;
}

/******************************************************************************/

void PhylogeneticsApplicationTools::optimizeParameters(
	TreeLikelihood * tl,
	map<string, string> & params,
	const string & suffix,
	bool suffixIsOptional,
	bool verbose)
	throw (Exception)
{
	bool optimize = ApplicationTools::getBooleanParameter("optimization", params, true, suffix, suffixIsOptional, false);
	if(!optimize) return;
		
	unsigned int optVerbose = ApplicationTools::getParameter<unsigned int>("optimization.verbose", params, 2, suffix, suffixIsOptional);
	
	string mhPath = ApplicationTools::getAFilePath("optimization.message_handler", params, false, false, suffix, suffixIsOptional);
	ostream * messageHandler = 
		(mhPath == "none") ? NULL :
			(mhPath == "std") ? &cout :
				new ofstream(mhPath.c_str(), ios::out);
	if(verbose) ApplicationTools::displayResult("Message handler", mhPath);

	string prPath = ApplicationTools::getAFilePath("optimization.profiler", params, false, false, suffix, suffixIsOptional);
	ostream * profiler = 
		(prPath == "none") ? NULL :
			(prPath == "std") ? &cout :
				new ofstream(prPath.c_str(), ios::out);
	if(profiler != NULL) (*profiler) << setprecision(20);
	if(verbose) ApplicationTools::displayResult("Profiler", prPath);

	bool scaleFirst = ApplicationTools::getBooleanParameter("optimization.scale_first", params, true, suffix, suffixIsOptional, true);
	if(scaleFirst) {
		// We scale the tree before optimizing each branch length separately:
		if(verbose) ApplicationTools::displayMessage("Scaling the tree before optimizing each branch length separately.");
		double tolerance = ApplicationTools::getDoubleParameter("optimization.scale_first.tolerance", params, .0001, suffix, suffixIsOptional, true);
		if(verbose) ApplicationTools::displayResult("Scaling tolerance", TextTools::toString(tolerance));
		int nbEvalMax = ApplicationTools::getIntParameter("optimization.scale_first.max_number_f_eval", params, 1000000, suffix, suffixIsOptional, true);
		if(verbose) ApplicationTools::displayResult("Scaling max # f eval", TextTools::toString(nbEvalMax));
		int n = OptimizationTools::optimizeTreeScale(
			tl,
			tolerance,
			nbEvalMax,
			messageHandler,
			profiler);
		if(verbose) ApplicationTools::message << "Performed " << n << " function evaluations." << endl;
	}
	
	// Should I ignore some parameters?
	string paramListDesc = ApplicationTools::getStringParameter("optimization.ignore_parameter", params, "", suffix, suffixIsOptional, false);
	StringTokenizer st(paramListDesc, ",");
	while(st.hasMoreToken()) {
		try {
			dynamic_cast<AbstractHomogeneousTreeLikelihood *>(tl) -> ignoreParameter(st.nextToken());
		} catch(ParameterNotFoundException pnfe) {
			ApplicationTools::displayError("Parameter '" + pnfe.getParameter() + "' not found, and so can't be ignored!");
		} catch(exception e) {
			ApplicationTools::error << "DEBUB: ERROR!!! This functionality can only be used with HomogeneousTreeLikelihood for now." << endl;
		}
	}
	
	int nbEvalMax = ApplicationTools::getIntParameter("optimization.max_number_f_eval", params, 1000000, suffix, suffixIsOptional);
	if(verbose) ApplicationTools::displayResult("Max # ML evaluations", TextTools::toString(nbEvalMax));
	
	double tolerance = ApplicationTools::getDoubleParameter("optimization.tolerance", params, .000001, suffix, suffixIsOptional);
	if(verbose) ApplicationTools::displayResult("Tolerance", TextTools::toString(tolerance));
	
	int n = OptimizationTools::optimizeNumericalParameters(
			dynamic_cast<AbstractHomogeneousTreeLikelihood *>(tl),
			tolerance,
			nbEvalMax,
			messageHandler,
			profiler,
			optVerbose);		
	if(verbose) ApplicationTools::displayResult("Performed", TextTools::toString(n) + " function evaluations.");
}

/******************************************************************************/

void PhylogeneticsApplicationTools::printOptimizationHelp() {
	ApplicationTools::message << "optimization                  | [yes/no] optimize parameters?" << endl;
	ApplicationTools::message << "optimization.verbose          | [0,1,2] level of verbose" << endl;
	ApplicationTools::message << "optimization.message_handler  | [none, std ot file path] where to dislay optimization messages" << endl;
	ApplicationTools::message << "                              | (if std, uses 'cout' to display messages)." << endl;
	ApplicationTools::message << "optimization.profiler         | [none, std ot file path] where to display optimization steps" << endl;
	ApplicationTools::message << "                              | (if std, uses 'cout' to display optimization steps)." << endl;
	ApplicationTools::message << "optimization.tolerance        | [double] tolerance parameter for stopping the estimation." << endl;
	ApplicationTools::message << "optimization.max_number_f_eval| [int] maximum number of likelihood computations." << endl;
	ApplicationTools::message << "optimization.ignore_parameter | [list] parameters to ignore during optimization." << endl;
	ApplicationTools::message << "optimization.scale_first      | [yes, no] tell if a global scale optimization must be done" << endl;
	ApplicationTools::message << "                              | prior to separate estimation of branch lengths." << endl;
	ApplicationTools::message << "optimization.scale_first      | " << endl;
	ApplicationTools::message << " .tolerance                   | [double] tolerance parameter for global scale optimization." << endl;
	ApplicationTools::message << " .max_number_f_eval           | [int] maximum number of computation for global scale optimization." << endl;
}

/******************************************************************************/

void PhylogeneticsApplicationTools::writeTree(
	const TreeTemplate<Node> & tree,
	map<string, string> & params,
	const string & suffix,
	bool verbose)
{
	string file = ApplicationTools::getAFilePath("output.tree", params, true, false, suffix, false);
	Newick newick;
	newick.write(tree, file, true);
	if(verbose) ApplicationTools::displayMessage("Wrote tree to file '" + file + "'.");
}

/******************************************************************************/

