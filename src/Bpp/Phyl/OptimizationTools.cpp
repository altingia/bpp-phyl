//
// File: OptimizationTools.cpp
// Created by: Julien Dutheil
// Created on: Sun Dec 14 09:43:32 2003
//

/*
Copyright or © or Copr. CNRS, (November 16, 2004)

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

#include "OptimizationTools.h"
#include "Likelihood/PseudoNewtonOptimizer.h"
#include "NNISearchable.h"
#include "NNITopologySearch.h"
#include "Io/Newick.h"

#include <Bpp/App/ApplicationTools.h>
#include <Bpp/Numeric/ParameterList.h>
#include <Bpp/Numeric/Function/PowellMultiDimensions.h>
#include <Bpp/Numeric/Function/SimpleNewtonMultiDimensions.h>
#include <Bpp/Numeric/Function/ConjugateGradientMultiDimensions.h>
#include <Bpp/Numeric/Function/DownhillSimplexMethod.h>
#include <Bpp/Numeric/Function/BrentOneDimension.h>
#include <Bpp/Numeric/Function/MetaOptimizer.h>
#include <Bpp/Numeric/Function/OptimizationStopCondition.h>
#include <Bpp/Numeric/Function/FivePointsNumericalDerivative.h>
#include <Bpp/Numeric/Function/ThreePointsNumericalDerivative.h>
#include <Bpp/Numeric/Function/TwoPointsNumericalDerivative.h>
#include <Bpp/Numeric/Function/ReparametrizationFunctionWrapper.h>

//From SeqLib:
#include <Bpp/Seq/Io/Fasta.h>

using namespace bpp;
using namespace std;

/******************************************************************************/

double NaNWatcher::getValue() const throw (Exception)
{
  double value = function_->getValue();
  if (isnan(value))
  {
    std::cerr << "Oups... something abnormal happened! A log file has been dumped to DEBUB.LOG" << std::endl;
    std::ofstream debug("DEBUG.LOG", std::ios::out);
    debug << "<<< DEBUGGING INFORMATION >>>" << std::endl;
    debug << "<<< SEND TO julien.dutheil@univ-montp2.fr >>>" << std::endl;
    debug << std::endl;
    debug << "<<< PARAMETERS >>>" << std::endl;
    function_->getParameters().printParameters(debug);
    debug << std::endl;
    debug << "<<< TREE >>>" << std::endl;
    Newick newick;
    newick.write(dynamic_cast<TreeLikelihood*>(function_)->getTree(), debug);
    debug << std::endl;
    debug << "<<< SEQUENCES >>>" << std::endl;
    Fasta fasta;
    fasta.write(debug, *dynamic_cast<TreeLikelihood*>(function_)->getData());
    debug.close();
    throw Exception("Optimization failed because likelihood function returned NaN.");
  }
  return value;
}

/******************************************************************************/

OptimizationTools::OptimizationTools() {}

OptimizationTools::~OptimizationTools() {}
 
/******************************************************************************/

std::string OptimizationTools::OPTIMIZATION_NEWTON = "newton";
std::string OptimizationTools::OPTIMIZATION_GRADIENT = "gradient";

/******************************************************************************/

OptimizationTools::ScaleFunction::ScaleFunction(TreeLikelihood* tl) :
  tl_(tl), brLen_(), lambda_()
{
  // We work only on the branch lengths:
  brLen_ = tl->getBranchLengthsParameters();
  if (brLen_.hasParameter("RootPosition"))
    brLen_.deleteParameter("RootPosition");
  lambda_.addParameter(Parameter("scale factor", 2.718282)); 
}
  
OptimizationTools::ScaleFunction::~ScaleFunction() {}

void OptimizationTools::ScaleFunction::setParameters(const ParameterList& lambda)
throw (ParameterNotFoundException, ConstraintException)
{
  if (lambda.size() != 1) throw Exception("OptimizationTools::ScaleFunction::f(). This is a one parameter function!");
  lambda_.setParametersValues(lambda);
}

double OptimizationTools::ScaleFunction::getValue() const
throw (ParameterException)
{
  // Scale the tree:
  ParameterList brLen = brLen_;
  double s = exp(lambda_[0].getValue());
  for(unsigned int i = 0; i < brLen.size(); i++)
  {
    brLen[i].setValue(brLen[i].getValue() * s);
  }
  return tl_->f(brLen);
}

/******************************************************************************/

unsigned int OptimizationTools::optimizeTreeScale(
  TreeLikelihood* tl,
  double tolerance,
  int tlEvalMax,
  OutputStream* messageHandler,
  OutputStream* profiler,
  unsigned int verbose)
throw (Exception)
{
  ScaleFunction sf(tl);
  BrentOneDimension bod(&sf);
  bod.setMessageHandler(messageHandler);
  bod.setProfiler(profiler);
  ParameterList singleParameter;
  singleParameter.addParameter(Parameter("scale factor", 2.718282));
  bod.setInitialInterval(2.7, 2.8);
  bod.init(singleParameter);
  ParametersStopCondition PS(&bod, tolerance);
  bod.setStopCondition(PS);
  bod.setMaximumNumberOfEvaluations(tlEvalMax);
  bod.optimize();
  ApplicationTools::displayTaskDone();
  if (verbose > 0)
    ApplicationTools::displayResult("Tree scaled by", exp(sf.getParameters()[0].getValue()));
  return bod.getNumberOfEvaluations();
}

/******************************************************************************/

unsigned int OptimizationTools::optimizeNumericalParameters(
  DiscreteRatesAcrossSitesTreeLikelihood* tl,
  const ParameterList& parameters,
  OptimizationListener* listener,
  unsigned int nstep,
  double tolerance,
  unsigned int tlEvalMax,
  OutputStream* messageHandler,
  OutputStream* profiler,
  bool reparametrization,
  unsigned int verbose,
  const std::string& optMethod)
throw (Exception)
{
  auto_ptr<DerivableSecondOrder> watcher(new NaNWatcher(tl));
  DerivableSecondOrder* f = watcher.get();
  ParameterList pl = parameters;
  //Shall we reparametrize the function to remove constraints?
  auto_ptr<DerivableSecondOrder> frep;
  if (reparametrization)
  {
    frep.reset(new ReparametrizationDerivableSecondOrderWrapper(f, parameters));
    f = frep.get();

    //Reset parameters to remove constraints:
    pl = f->getParameters().subList(parameters.getParameterNames());
  }

  // Build optimizer:
  MetaOptimizerInfos* desc = new MetaOptimizerInfos();
  if (optMethod == OPTIMIZATION_GRADIENT)
    desc->addOptimizer("Branch length parameters", new ConjugateGradientMultiDimensions(f), tl->getBranchLengthsParameters().getParameterNames(), 2, MetaOptimizerInfos::IT_TYPE_FULL);
  else if (optMethod == OPTIMIZATION_NEWTON)
    desc->addOptimizer("Branch length parameters", new PseudoNewtonOptimizer(f), tl->getBranchLengthsParameters().getParameterNames(), 2, MetaOptimizerInfos::IT_TYPE_FULL);
  else throw Exception("OptimizationTools::optimizeNumericalParameters. Unknown optimization method: " + optMethod);
  
  ParameterList plsm = parameters.getCommonParametersWith(tl->getSubstitutionModelParameters());
  desc->addOptimizer("Substitution model parameter", new SimpleMultiDimensions(f), plsm.getParameterNames(), 0, MetaOptimizerInfos::IT_TYPE_STEP);
  

  ParameterList plrd = parameters.getCommonParametersWith(tl->getRateDistributionParameters());
  desc->addOptimizer("Rate distribution parameter", new SimpleMultiDimensions(f), plrd.getParameterNames(), 0, MetaOptimizerInfos::IT_TYPE_STEP);
  

  MetaOptimizer optimizer(f, desc, nstep);
  optimizer.setVerbose(verbose);
  optimizer.setProfiler(profiler);
  optimizer.setMessageHandler(messageHandler);
  optimizer.setMaximumNumberOfEvaluations(tlEvalMax);
  optimizer.getStopCondition()->setTolerance(tolerance);
  
  // Optimize TreeLikelihood function:
  optimizer.setConstraintPolicy(AutoParameter::CONSTRAINTS_AUTO);
  if (listener) cout << "ben si pourtant" << endl;
  if (listener) optimizer.addOptimizationListener(listener);
  optimizer.init(pl);
  optimizer.optimize();
  if (verbose > 0) ApplicationTools::displayMessage("\n");

  // We're done.
  return optimizer.getNumberOfEvaluations(); 
}
  
/******************************************************************************/

unsigned int OptimizationTools::optimizeNumericalParameters2(
  DiscreteRatesAcrossSitesTreeLikelihood* tl,
  const ParameterList& parameters,
  OptimizationListener* listener,
  double tolerance,
  unsigned int tlEvalMax,
  OutputStream* messageHandler,
  OutputStream* profiler,
  bool reparametrization,
  unsigned int verbose,
  const std::string& optMethod)
throw (Exception)
{
  DerivableSecondOrder* f = tl;
  ParameterList pl = parameters;
  //Shall we reparametrize the function to remove constraints?
  auto_ptr<DerivableSecondOrder> frep;
  if (reparametrization)
  {
    frep.reset(new ReparametrizationDerivableSecondOrderWrapper(tl, parameters));
    f = frep.get();

    //Reset parameters to remove constraints:
    pl = f->getParameters().subList(parameters.getParameterNames());
  }

  auto_ptr<AbstractNumericalDerivative> fnum;
  // Build optimizer:
  auto_ptr<Optimizer> optimizer;
  if (optMethod == OPTIMIZATION_GRADIENT)
  {
    fnum.reset(new TwoPointsNumericalDerivative(f));
    fnum->setInterval(0.0000001);
    optimizer.reset(new ConjugateGradientMultiDimensions(reinterpret_cast<DerivableFirstOrder *>(fnum.get()))); //Removes strict-aliasing warning with gcc 4.4
  }
  else if (optMethod == OPTIMIZATION_NEWTON)
  {
    fnum.reset(new ThreePointsNumericalDerivative(f));
    fnum->setInterval(0.0001);
    optimizer.reset(new PseudoNewtonOptimizer(fnum.get()));
  }
  else throw Exception("OptimizationTools::optimizeNumericalParameters2. Unknown optimization method: " + optMethod);
  
  //Numerical derivatives:
  ParameterList tmp = parameters.getCommonParametersWith(tl->getSubstitutionModelParameters());
  tmp.addParameters(parameters.getCommonParametersWith(tl->getRateDistributionParameters()));
  fnum->setParametersToDerivate(tmp.getParameterNames());
 
  optimizer->setVerbose(verbose);
  optimizer->setProfiler(profiler);
  optimizer->setMessageHandler(messageHandler);
  optimizer->setMaximumNumberOfEvaluations(tlEvalMax);
  optimizer->getStopCondition()->setTolerance(tolerance);
  
  // Optimize TreeLikelihood function:
  optimizer->setConstraintPolicy(AutoParameter::CONSTRAINTS_AUTO);
  if (listener) optimizer->addOptimizationListener(listener);
  optimizer->init(pl);
  optimizer->optimize();
  if (verbose > 0) ApplicationTools::displayMessage("\n");
  
  // We're done.
  unsigned int n = optimizer->getNumberOfEvaluations(); 

  if (verbose > 0) ApplicationTools::displayMessage("\n");
  
  return n;
}

/******************************************************************************/

unsigned int OptimizationTools::optimizeBranchLengthsParameters(
  DiscreteRatesAcrossSitesTreeLikelihood* tl,
  const ParameterList& parameters,
  OptimizationListener* listener,
  double tolerance,
  unsigned int tlEvalMax,
  OutputStream* messageHandler,
  OutputStream* profiler,
  unsigned int verbose,
  const std::string& optMethod)
throw (Exception)
{
  // Build optimizer:
  Optimizer* optimizer = 0;
  if(optMethod == OPTIMIZATION_GRADIENT)
    optimizer = new ConjugateGradientMultiDimensions(tl);
  else if(optMethod == OPTIMIZATION_NEWTON)
    optimizer = new PseudoNewtonOptimizer(tl);
  else throw Exception("OptimizationTools::optimizeBranchLengthsParameters. Unknown optimization method: " + optMethod);
  optimizer->setVerbose(verbose);
  optimizer->setProfiler(profiler);
  optimizer->setMessageHandler(messageHandler);
  optimizer->setMaximumNumberOfEvaluations(tlEvalMax);
  optimizer->getStopCondition()->setTolerance(tolerance);
  
  // Optimize TreeLikelihood function:
  ParameterList pl = parameters.getCommonParametersWith(tl->getBranchLengthsParameters());
  optimizer->setConstraintPolicy(AutoParameter::CONSTRAINTS_AUTO);
  if(listener) optimizer->addOptimizationListener(listener);
  optimizer->init(pl);
  optimizer->optimize();
  if(verbose > 0) ApplicationTools::displayMessage("\n");
  
  // We're done.
  unsigned int n = optimizer->getNumberOfEvaluations();
  delete optimizer;
  return n;
}
  
/******************************************************************************/

unsigned int OptimizationTools::optimizeNumericalParametersWithGlobalClock(
  DiscreteRatesAcrossSitesClockTreeLikelihood* cl,
  const ParameterList& parameters,
  OptimizationListener* listener,
  unsigned int nstep,
  double tolerance,
  unsigned int tlEvalMax,
  OutputStream* messageHandler,
  OutputStream* profiler,
  unsigned int verbose,
  const std::string& optMethod)
throw (Exception)
{
  AbstractNumericalDerivative* fun = 0;

  // Build optimizer:
  MetaOptimizerInfos* desc = new MetaOptimizerInfos();
  if(optMethod == OPTIMIZATION_GRADIENT)
  {
    fun = new TwoPointsNumericalDerivative(cl);
    fun->setInterval(0.0000001);
    desc->addOptimizer("Branch length parameters", new ConjugateGradientMultiDimensions(fun), cl->getBranchLengthsParameters().getParameterNames(), 2, MetaOptimizerInfos::IT_TYPE_FULL);
  }
  else if(optMethod == OPTIMIZATION_NEWTON) 
  {
    fun = new ThreePointsNumericalDerivative(cl);
    fun->setInterval(0.0001);
    desc->addOptimizer("Branch length parameters", new PseudoNewtonOptimizer(fun), cl->getBranchLengthsParameters().getParameterNames(), 2, MetaOptimizerInfos::IT_TYPE_FULL);
  }
  else throw Exception("OptimizationTools::optimizeNumericalParametersWithGlobalClock. Unknown optimization method: " + optMethod);

  //Numerical derivatives:
  ParameterList tmp = parameters.getCommonParametersWith(cl->getBranchLengthsParameters());
  fun->setParametersToDerivate(tmp.getParameterNames());

  ParameterList plsm = parameters.getCommonParametersWith(cl->getSubstitutionModelParameters());
  if(plsm.size() < 10)
    desc->addOptimizer("Substitution model parameter", new SimpleMultiDimensions(cl), plsm.getParameterNames(), 0, MetaOptimizerInfos::IT_TYPE_STEP);
  else
    desc->addOptimizer("Substitution model parameters", new DownhillSimplexMethod(cl), plsm.getParameterNames(), 0, MetaOptimizerInfos::IT_TYPE_FULL);
  
  ParameterList plrd = parameters.getCommonParametersWith(cl->getRateDistributionParameters());
  if(plrd.size() < 10)
    desc->addOptimizer("Rate distribution parameter", new SimpleMultiDimensions(cl), plrd.getParameterNames(), 0, MetaOptimizerInfos::IT_TYPE_STEP);
  else
    desc->addOptimizer("Rate dsitribution parameters", new DownhillSimplexMethod(cl), plrd.getParameterNames(), 0, MetaOptimizerInfos::IT_TYPE_FULL);
 
  MetaOptimizer optimizer(fun, desc, nstep);
  optimizer.setVerbose(verbose);
  optimizer.setProfiler(profiler);
  optimizer.setMessageHandler(messageHandler);
  optimizer.setMaximumNumberOfEvaluations(tlEvalMax);
  optimizer.getStopCondition()->setTolerance(tolerance);
  
  // Optimize TreeLikelihood function:
  optimizer.setConstraintPolicy(AutoParameter::CONSTRAINTS_AUTO);
  if(listener) optimizer.addOptimizationListener(listener);
  optimizer.init(parameters);
  optimizer.optimize();
  if(verbose > 0) ApplicationTools::displayMessage("\n");
  
  // We're done.
  return optimizer.getNumberOfEvaluations(); 
}

/******************************************************************************/

unsigned int OptimizationTools::optimizeNumericalParametersWithGlobalClock2(
  DiscreteRatesAcrossSitesClockTreeLikelihood* cl,
  const ParameterList& parameters,
  OptimizationListener* listener,
  double tolerance,
  unsigned int tlEvalMax,
  OutputStream* messageHandler,
  OutputStream* profiler,
  unsigned int verbose,
  const std::string& optMethod)
throw (Exception)
{
  AbstractNumericalDerivative* fun = 0;

  // Build optimizer:
  Optimizer* optimizer = 0;
  if(optMethod == OPTIMIZATION_GRADIENT)
  {
    fun = new TwoPointsNumericalDerivative(cl);
    fun->setInterval(0.0000001);
    optimizer = new ConjugateGradientMultiDimensions(fun);
  }
  else if(optMethod == OPTIMIZATION_NEWTON)
  {
    fun = new ThreePointsNumericalDerivative(cl);
    fun->setInterval(0.0001);
    optimizer = new PseudoNewtonOptimizer(fun);
  }
  else throw Exception("OptimizationTools::optimizeBranchLengthsParameters. Unknown optimization method: " + optMethod);
  
  //Numerical derivatives:
  ParameterList tmp = parameters.getCommonParametersWith(cl->getParameters());
  fun->setParametersToDerivate(tmp.getParameterNames());

  optimizer->setVerbose(verbose);
  optimizer->setProfiler(profiler);
  optimizer->setMessageHandler(messageHandler);
  optimizer->setMaximumNumberOfEvaluations(tlEvalMax);
  optimizer->getStopCondition()->setTolerance(tolerance);

  // Optimize TreeLikelihood function:
  optimizer->setConstraintPolicy(AutoParameter::CONSTRAINTS_AUTO);
  if(listener) optimizer->addOptimizationListener(listener);
  optimizer->init(parameters);
  optimizer->optimize();
  if(verbose > 0) ApplicationTools::displayMessage("\n");
  
  // We're done.
  unsigned int n = optimizer->getNumberOfEvaluations(); 
  delete optimizer;

  // We're done.
  return n; 
}

/******************************************************************************/  

void NNITopologyListener::topologyChangeSuccessful(const TopologyChangeEvent& event)
{
  optimizeCounter_++;
  if (optimizeCounter_ == optimizeNumerical_)
  {
    DiscreteRatesAcrossSitesTreeLikelihood* likelihood = dynamic_cast<DiscreteRatesAcrossSitesTreeLikelihood *>(topoSearch_->getSearchableObject());
    parameters_.matchParametersValues(likelihood->getParameters());
    OptimizationTools::optimizeNumericalParameters(likelihood, parameters_, 0, nStep_, tolerance_, 1000000, messenger_, profiler_, reparametrization_, verbose_, optMethod_);
    optimizeCounter_ = 0;
  }
}

/******************************************************************************/  

void NNITopologyListener2::topologyChangeSuccessful(const TopologyChangeEvent & event)
{
  optimizeCounter_++;
  if (optimizeCounter_ == optimizeNumerical_)
  {
    DiscreteRatesAcrossSitesTreeLikelihood* likelihood = dynamic_cast<DiscreteRatesAcrossSitesTreeLikelihood *>(topoSearch_->getSearchableObject());
    parameters_.matchParametersValues(likelihood->getParameters());
    OptimizationTools::optimizeNumericalParameters2(likelihood, parameters_, 0, tolerance_, 1000000, messenger_, profiler_, reparametrization_, verbose_, optMethod_);
    optimizeCounter_ = 0;
  }
}

//******************************************************************************/  

NNIHomogeneousTreeLikelihood* OptimizationTools::optimizeTreeNNI(
    NNIHomogeneousTreeLikelihood* tl,
    const ParameterList& parameters,
    bool optimizeNumFirst,
    double tolBefore,
    double tolDuring,
    int tlEvalMax,
    unsigned int numStep,
    OutputStream* messageHandler,
    OutputStream* profiler,
    bool reparametrization,
    unsigned int verbose,
    const std::string& optMethod,
    unsigned int nStep,
    const std::string& nniMethod)
  throw (Exception)
{
  //Roughly optimize parameter
  if (optimizeNumFirst)
  {
    OptimizationTools::optimizeNumericalParameters(tl, parameters, NULL, nStep, tolBefore, 1000000, messageHandler, profiler, reparametrization, verbose, optMethod);
  }
  //Begin topo search:
  NNITopologySearch topoSearch(*tl, nniMethod, verbose > 2 ? verbose - 2 : 0);
  NNITopologyListener *topoListener = new NNITopologyListener(&topoSearch, parameters, tolDuring, messageHandler, profiler, verbose, optMethod, nStep, reparametrization);
  topoListener->setNumericalOptimizationCounter(numStep);
  topoSearch.addTopologyListener(topoListener);
  topoSearch.search();
  return dynamic_cast<NNIHomogeneousTreeLikelihood *>(topoSearch.getSearchableObject());
}

/******************************************************************************/  

NNIHomogeneousTreeLikelihood* OptimizationTools::optimizeTreeNNI2(
    NNIHomogeneousTreeLikelihood* tl,
    const ParameterList& parameters,
    bool optimizeNumFirst,
    double tolBefore,
    double tolDuring,
    int tlEvalMax,
    unsigned int numStep,
    OutputStream* messageHandler,
    OutputStream* profiler,
    bool reparametrization,
    unsigned int verbose,
    const std::string& optMethod,
    const std::string& nniMethod)
  throw (Exception)
{
  //Roughly optimize parameter
  if (optimizeNumFirst)
  {
    OptimizationTools::optimizeNumericalParameters2(tl, parameters, NULL, tolBefore, 1000000, messageHandler, profiler, reparametrization, verbose, optMethod);
  }
  //Begin topo search:
  NNITopologySearch topoSearch(*tl, nniMethod, verbose > 2 ? verbose - 2 : 0);
  NNITopologyListener2 *topoListener = new NNITopologyListener2(&topoSearch, parameters, tolDuring, messageHandler, profiler, verbose, optMethod, reparametrization);
  topoListener->setNumericalOptimizationCounter(numStep);
  topoSearch.addTopologyListener(topoListener);
  topoSearch.search();
  return dynamic_cast<NNIHomogeneousTreeLikelihood *>(topoSearch.getSearchableObject());
}

/******************************************************************************/  

DRTreeParsimonyScore* OptimizationTools::optimizeTreeNNI(
        DRTreeParsimonyScore* tp,
        unsigned int verbose)  
{
  NNISearchable* topo = dynamic_cast<NNISearchable*>(tp);
  NNITopologySearch topoSearch(*topo, NNITopologySearch::PHYML, verbose);
  topoSearch.search();
  return dynamic_cast<DRTreeParsimonyScore*>(topoSearch.getSearchableObject());
};

/******************************************************************************/  

std::string OptimizationTools::DISTANCEMETHOD_INIT       = "init";
std::string OptimizationTools::DISTANCEMETHOD_PAIRWISE   = "pairwise";
std::string OptimizationTools::DISTANCEMETHOD_ITERATIONS = "iterations";

/******************************************************************************/  

TreeTemplate<Node>* OptimizationTools::buildDistanceTree(
    DistanceEstimation& estimationMethod,
    AgglomerativeDistanceMethod& reconstructionMethod,
    const ParameterList& parametersToIgnore,
    bool optimizeBrLen,
    bool rooted,
    const std::string& param,
    double tolerance,
    unsigned int tlEvalMax,
    OutputStream* profiler,
    OutputStream* messenger,
    unsigned int verbose) throw (Exception)
{
  estimationMethod.resetAdditionalParameters();
  estimationMethod.setVerbose(verbose);
  if(param == DISTANCEMETHOD_PAIRWISE)
  {
    ParameterList tmp = estimationMethod.getModel()->getIndependentParameters();
    tmp.addParameters(estimationMethod.getRateDistribution()->getIndependentParameters());
    tmp.deleteParameters(parametersToIgnore.getParameterNames());
    estimationMethod.setAdditionalParameters(tmp);
  }
  TreeTemplate<Node> * tree = NULL;
  TreeTemplate<Node> * previousTree = NULL;
  bool test = true;
  while(test)
  {
    //Compute matrice:
    if(verbose > 0)
      ApplicationTools::displayTask("Estimating distance matrix", true); 
    estimationMethod.computeMatrix();
    DistanceMatrix * matrix = estimationMethod.getMatrix();
    if(verbose > 0)
      ApplicationTools::displayTaskDone();

    //Compute tree:
    if(verbose > 0)
      ApplicationTools::displayTask("Building tree");
    reconstructionMethod.setDistanceMatrix(*matrix);
    reconstructionMethod.computeTree(rooted);
    previousTree = tree;
    delete matrix;
    tree = dynamic_cast<TreeTemplate<Node> *>(reconstructionMethod.getTree());
    if(verbose > 0)
      ApplicationTools::displayTaskDone();
    if(previousTree && verbose > 0)
    {
      int rf = TreeTools::robinsonFouldsDistance(*previousTree, *tree, false);
      ApplicationTools::displayResult("Topo. distance with previous iteration", TextTools::toString(rf));
      test = (rf == 0);
      delete previousTree;
    }
    if(param != DISTANCEMETHOD_ITERATIONS) break; //Ends here.
    
    //Now, re-estimate parameters:
    DRHomogeneousTreeLikelihood tl(*tree, *estimationMethod.getData(), estimationMethod.getModel(), estimationMethod.getRateDistribution(), true, verbose > 1);
    tl.initialize();
    ParameterList parameters = tl.getParameters();
    if (!optimizeBrLen)
    {
      vector<string> vs = tl.getBranchLengthsParameters().getParameterNames();
      parameters.deleteParameters(vs);
    }
    parameters.deleteParameters(parametersToIgnore.getParameterNames());
    optimizeNumericalParameters(&tl, parameters, NULL, 0, tolerance, tlEvalMax, messenger, profiler, verbose > 0 ? verbose - 1 : 0);
    if(verbose > 0)
    {
      ParameterList tmp = tl.getSubstitutionModelParameters();
      for(unsigned int i = 0; i < tmp.size(); i++)
        ApplicationTools::displayResult(tmp[i].getName(), TextTools::toString(tmp[i].getValue()));
      tmp = tl.getRateDistributionParameters();
      for(unsigned int i = 0; i < tmp.size(); i++)
        ApplicationTools::displayResult(tmp[i].getName(), TextTools::toString(tmp[i].getValue()));
    }
  }
  return tree;
}

/******************************************************************************/  
