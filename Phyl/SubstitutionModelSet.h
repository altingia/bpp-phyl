//
// File: SubstitutionModelSet.h
// Created by: Bastien Boussau
//             Julien Dutheil
// Created on: Tue Aug 21 2007
//

/*
Copyright or <A9> or Copr. CNRS, (November 16, 2004)

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

#ifndef _SUBSTITUTIONMODELSET_H_
#define _SUBSTITUTIONMODELSET_H_


#include "Tree.h"
#include "SubstitutionModel.h"
#include "AbstractSubstitutionModel.h"

// From NumCalc:
#include <NumCalc/RandomTools.h>
#include <NumCalc/VectorTools.h>

// From utils:
#include <Utils/Exceptions.h>

//From Seqlib:
#include <Seq/Alphabet.h>
#include <Seq/NucleicAlphabet.h>

// From the STL:
#include <vector>
#include <map>
#include <algorithm>
using namespace std;

class FrequenciesSet:
  public virtual Parametrizable
{
  public:
#ifndef NO_VIRTUAL_COV
    FrequenciesSet * clone() const = 0;
#endif
  public:
    virtual const Alphabet * getAlphabet() const = 0;
    virtual vector<double> getFrequencies() const = 0;
    virtual unsigned int getNumberOfParameters() const = 0;
};

class AbstractFrequenciesSet:
  public virtual FrequenciesSet,
  public AbstractParametrizable
{
  protected:
    const Alphabet * _alphabet;
    vector<double> _freq;

  public:
    AbstractFrequenciesSet(const Alphabet * alphabet): _alphabet(alphabet) {}
  
  public:
    const Alphabet * getAlphabet() const { return _alphabet; }
    vector<double> getFrequencies() const { return _freq; }
    unsigned int getNumberOfParameters() const { return _parameters.size(); }
};

class FullFrequenciesSet:
  public AbstractFrequenciesSet
{
  public:
    FullFrequenciesSet(const Alphabet * alphabet, const string & prefix = ""):
      AbstractFrequenciesSet(alphabet)
    {
      _freq.resize(alphabet->getSize());
      for(unsigned int i = 0; i < alphabet->getSize(); i++)
      {
        _parameters.addParameter(Parameter(prefix + alphabet->intToChar((int)i), 1. / alphabet->getSize(), &Parameter::PROP_CONSTRAINT_IN));
        _freq[i] = 1. / alphabet->getSize();
      }
    }
    FullFrequenciesSet(const Alphabet * alphabet, const vector<double> & initFreqs, const string & prefix = "") throw (Exception):
      AbstractFrequenciesSet(alphabet)
    {
      if(initFreqs.size() != alphabet->getSize())
        throw Exception("FullFrequenciesSet(constructor). There must be " + TextTools::toString(alphabet->getSize()) + " frequencies.");
      double sum = 0.0;
      for(unsigned int i = 0; i < initFreqs.size(); i++)
      {
        sum += initFreqs[i];
      }
      if(fabs(1-sum) > 0.00000000000001)
      {
        throw Exception("Root frequencies must equal 1.");
      }
      _freq.resize(alphabet->getSize());
      for(unsigned int i = 0; i < alphabet->getSize(); i++)
      {
        _parameters.addParameter(Parameter(prefix + alphabet->intToChar((int)i), initFreqs[i], &Parameter::PROP_CONSTRAINT_IN));
        _freq[i] = initFreqs[i];
      }
    }
#ifndef NO_VIRTUAL_COV
    FullFrequenciesSet *
#else
    Clonable *
#endif
    clone() const { return new FullFrequenciesSet(*this); }

  public:
    void fireParameterChanged(const ParameterList & pl)
    {
      for(unsigned int i = 0; i < _alphabet->getSize(); i++)
      {
        _freq[i] = _parameters[i]->getValue();
      }
    }
};

class GCFrequenciesSet:
  public AbstractFrequenciesSet
{
  public:
    GCFrequenciesSet(const NucleicAlphabet * alphabet, const string & prefix = ""):
      AbstractFrequenciesSet(alphabet)
    {
      _freq.resize(4);
      _parameters.addParameter(Parameter(prefix + "theta", 0.5, &Parameter::PROP_CONSTRAINT_IN));
      _freq[0] = _freq[1] = _freq[2] = _freq[3] = 0.25;
    }
    GCFrequenciesSet(const NucleicAlphabet * alphabet, double theta, const string & prefix = ""):
      AbstractFrequenciesSet(alphabet)
    {
      _freq.resize(4);
      _parameters.addParameter(Parameter(prefix + "theta", theta, &Parameter::PROP_CONSTRAINT_IN));
      _freq[0] = _freq[3] = (1. - theta) / 2.;
      _freq[1] = _freq[2] = theta / 2.;
    }
#ifndef NO_VIRTUAL_COV
    GCFrequenciesSet *
#else
    Clonable *
#endif
    clone() const { return new GCFrequenciesSet(*this); }

  public:
    void fireParameterChanged(const ParameterList & pl)
    {
      double theta = _parameters[0]->getValue();
      _freq[0] = _freq[3] = (1. - theta) / 2.;
      _freq[1] = _freq[2] = theta / 2.;
    }
};

class MarkovModulatedFrequenciesSet:
  public AbstractFrequenciesSet
{
  protected:
    FrequenciesSet * _freqSet;
    vector<double> _rateFreqs;
  public:
    MarkovModulatedFrequenciesSet(const MarkovModulatedFrequenciesSet & mmfs):
      AbstractFrequenciesSet(mmfs)
    {
      _freqSet = dynamic_cast<FrequenciesSet *>(mmfs._freqSet->clone());
      _rateFreqs = mmfs._rateFreqs;
    }
    MarkovModulatedFrequenciesSet & operator=(const MarkovModulatedFrequenciesSet & mmfs)
    {
      AbstractFrequenciesSet::operator=(mmfs);
      _freqSet = dynamic_cast<FrequenciesSet *>(mmfs._freqSet->clone());
      _rateFreqs = mmfs._rateFreqs;
      return *this;
    }
    MarkovModulatedFrequenciesSet(FrequenciesSet * freqSet, const vector<double> & rateFreqs):
      AbstractFrequenciesSet(freqSet->getAlphabet()), _rateFreqs(rateFreqs)
    {
      _freq.resize(_alphabet->getSize() * rateFreqs.size());
      _parameters.addParameters(_freqSet->getParameters());
      _freq = VectorTools::kroneckerMult(rateFreqs, _freqSet->getFrequencies());
    }
#ifndef NO_VIRTUAL_COV
    MarkovModulatedFrequenciesSet *
#else
    Clonable *
#endif
    clone() const { return new MarkovModulatedFrequenciesSet(*this); }

    virtual ~MarkovModulatedFrequenciesSet() { delete _freqSet; }

  public:
    void fireParameterChanged(const ParameterList & pl)
    {
      _freqSet->matchParametersValues(pl);
      _freq = VectorTools::kroneckerMult(_rateFreqs, _freqSet->getFrequencies());
    }
};


/**
 * @brief Substitution models manager for heterogenous models of evolution.
 *
 * This class contains a set of substitution models, and their assigment towoard the branches of a phylogenetic tree.
 * Each branch in the tree corresponds to a model in the set, but a susbstitution model may correspond to several branches.
 * The particular case where all branche points toward a unique model is the homogeneous case.
 *
 * This class also deals with the parameters associated to the models.
 * In the homogeneous case, the parameter list is the same as the list in susbstitution model.
 * When two models at least are specified, these model may have their own parameters or share some of them.
 * To deal with this issue, the SubstitutionModelSet class contains its own parameter list and an index wich tells to which
 * models this parameter applies to.
 * Since parameters in a list must have unique names, duplicated names are numbered according to the order in the list.
 * To track the relation between names in the list and names in each model, the parameter list is duplicated in _modelParameters.
 * The user only act on _parameters, the fireParameterChanged function, automatically called, will update the _modelParameters field.
 */
class SubstitutionModelSet: 
  public AbstractParametrizable
{
  protected:
    /**
     * @brief A pointer toward the comon alphabet to all models in the set.
     */
    const Alphabet * _alphabet;

    /**
     * @brief contains all models used in this tree.
     */
    vector<SubstitutionModel *> _modelSet;

    /**
     * @brief Root frequencies.
     */
    FrequenciesSet * _rootFrequencies;

    /**
     * @brief contains for each node in a tree the index of the corresponding model in _modelSet
     */
    mutable map<int, unsigned int> _nodeToModel;
    mutable map<unsigned int, int> _modelToNode;

    /**
     * @brief contains for each parameter in the list the indexes of the corresponding models in _modelSet that share this parameter.
     */
    vector< vector<unsigned int> > _paramToModels;

    map<string, unsigned int> _paramNamesCount;

    /**
     * @brief contains for each parameter in the list the corresponding name in substitution models.
     */
    vector<string> _modelParameterNames;

    /**
     * @brief Parameters for each model in the set.
     *
     * The _parameters field, inherited from AbstractSubstitutionModel contains all parameters, with unique names.
     * To make the correspondance with parameters for each model in the set, we duplicate them in this array.
     * In most cases, this is something like 'theta_1 <=> theta', 'theta_2 <=> theta', etc.
     */
    vector<ParameterList> _modelParameters;

  public:
  
    SubstitutionModelSet(const Alphabet *alpha): _alphabet(alpha)
    {
      _rootFrequencies = new FullFrequenciesSet(alpha, "RootFreq");
      _parameters.addParameters(_rootFrequencies->getParameters());
    }

    SubstitutionModelSet(const Alphabet *alpha, FrequenciesSet * rootFreqs): _alphabet(alpha), _rootFrequencies(rootFreqs)
    {
      _parameters.addParameters(_rootFrequencies->getParameters());
    }

    SubstitutionModelSet(const SubstitutionModelSet & set);
    
    SubstitutionModelSet & operator=(const SubstitutionModelSet & set);
    
    virtual ~SubstitutionModelSet()
    {
      for(unsigned int i = 0; i < _modelSet.size(); i++) delete _modelSet[i];
      delete _rootFrequencies;
    }

    SubstitutionModelSet * clone() const { return new SubstitutionModelSet(*this); }

  public:

    /**
     * @brief Get the number of states associated to this model set.
     *
     * @return The number of states.
     * @throw Exception if no model is associated to the set.
     */
    unsigned int getNumberOfStates() const throw (Exception)
    {
      return _rootFrequencies->getFrequencies().size();
    }

    unsigned int getParameterIndex(const string & name) const throw (ParameterNotFoundException)
    {
      for(unsigned int i = 0; i < _parameters.size(); i++)
        if(_parameters[i]->getName() == name) return i;
      throw ParameterNotFoundException("SubstitutionModelSet::getParameterIndex.", name);
    }

    /**
     * To be called when a parameter has changed.
     * Depending on parameters, this will actualize the _initialFrequencies vector or the corresponding models in the set.
     * @param parameters The modified parameters.
     */
    void fireParameterChanged(const ParameterList & parameters);

    /**
     * @return The current number of distinct substitution models in this set.
     */
    unsigned int getNumberOfModels() const { return _modelSet.size(); }

    /**
     * @brief Get one model from the set knowing its index.
     *
     * @param i Index of the model in the set.
     * @return A pointer toward the corresponding model.
     */
    const SubstitutionModel * getModel(unsigned int i) const throw (IndexOutOfBoundsException)
    {
      if(i > _modelSet.size()) throw IndexOutOfBoundsException("SubstitutionModelSet::getNumberOfModels().", 0, _modelSet.size()-1, i);
      return _modelSet[i];
    }

    /**
     * @brief Get the model associated to a particular node id.
     *
     * @param nodeId The id of the query node.
     * @return A pointer toward the corresponding model.
     * @throw Exception If no model is found for this node.
     */
    const SubstitutionModel * getModelForNode(int nodeId) const throw (Exception)
    {
      map<int, unsigned int>::iterator i = _nodeToModel.find(nodeId);
      if(i == _nodeToModel.end())
        throw Exception("SubstitutionModelSet::getModelForNode(). No model associated to node with id " + TextTools::toString(nodeId));
      return _modelSet[i->second];
    }
    SubstitutionModel * getModelForNode(int nodeId) throw (Exception)
    {
      map<int, unsigned int>::const_iterator i = _nodeToModel.find(nodeId);
      if(i == _nodeToModel.end())
        throw Exception("SubstitutionModelSet::getModelForNode(). No model associated to node with id " + TextTools::toString(nodeId));
      return _modelSet[i->second];
    }

    /**
     * @return The list of nodes with a model containing the specified parameter.
     */
    vector<int> getNodesWithParameter(const string & name) const
    {
      vector<int> ids;
      unsigned int offset = _rootFrequencies->getNumberOfParameters();
      for(unsigned int i = 0; i < _paramToModels.size(); i++)
      {
        if(_parameters[offset + i]->getName() == name)
        {
          ids.resize(_paramToModels[i].size());
          for(unsigned int j = 0; j < ids.size(); j++)
            ids[j] = _modelToNode[_paramToModels[i][j]];
          return ids;
        }
      }
      return ids;
    }

    /**
     * @brief Add a new model to the set, and set relationships with nodes and params.
     *
     * @param model A pointer toward a susbstitution model, that will added to the set.
     * Warning! The set will now be the owner of the pointer, and will destroy it if needed!
     * Copy the model first if you don't want it to be lost!
     * @param nodesId the set of nodes in the tree that points toward this model.
     * This will override any previous affectation.
     * @param newParams The names of the parameters that have to be added to the global list.
     * These parameters will only be affected to this susbstitution model.
     * You can use the setParameterToModel function to assign this parameter to an additional model, and the
     * unsetParameterToModel to remove the relationship with this model for instance.
     * Parameters not specified in newParams will be ignored, unless you manually assign them to another parameter with
     * setParameterToModel.
     * @throw Exception in case of error:
     * <ul>
     * <li>if the new model does not match the alphabet<li>
     * <li>if the new model does not have the same number of states than existing ones<li>
     * <li>etc.</li>
     * </ul>
     */
    void addModel(SubstitutionModel * model, const vector<int> & nodesId, const vector<string> & newParams) throw (Exception);
  
    /**
     * @brief Change a given model.
     *
     * The new model will be copied and will replace the old one.
     * All previous associations will be kept the same.
     * @param model A pointer toward a susbstitution model, that will added to the set.
     * Warning! The set will now be the owner of the pointer, and will destroy it if needed!
     * Copy the model first if you don't want it to be lost!
     * @param modelIndex The index of the existing model to replace.
     */
    void setModel(SubstitutionModel * model, unsigned int modelIndex) throw (Exception, IndexOutOfBoundsException);
 
    /**
     * @brief Associate an existing model with a given node.
     *
     * If the node was was previously associated to a model, the old association is deleted.
     * If other nodes are associated to this model, the association is conserved.
     *
     * @param modelIndex The position of the model in the set.
     * @param nodeNumber The id of the corresponding node.
     */
    void setModelToNode(unsigned int modelIndex, int nodeNumber) throw (IndexOutOfBoundsException)
    {
      if(modelIndex >= _nodeToModel.size()) throw IndexOutOfBoundsException("SubstitutionModelSet::setModelToNode.", modelIndex, 0, _nodeToModel.size() - 1);
      _nodeToModel[nodeNumber] = modelIndex;
    }
    
    void setParameterToModel(unsigned int parameterIndex, unsigned int modelIndex) throw (IndexOutOfBoundsException)
    {
      if(parameterIndex >= _paramToModels.size()) throw IndexOutOfBoundsException("SubstitutionModelSet::setParameterToModel.", parameterIndex, 0, _paramToModels.size() - 1);
      _paramToModels[parameterIndex].push_back(modelIndex);
    }

    void unsetParameterToModel(unsigned int parameterIndex, unsigned int modelIndex) throw (IndexOutOfBoundsException)
    {
      if(parameterIndex >= _paramToModels.size()) throw IndexOutOfBoundsException("SubstitutionModelSet::unsetParameterToModel.", parameterIndex, 0, _paramToModels.size() - 1);
      remove(_paramToModels[parameterIndex].begin(), _paramToModels[parameterIndex].end(), modelIndex);
      if(!checkOrphanModels())     throw Exception("DEBUG: SubstitutionModelSet::unsetParameterToModel. Orphan model!");
      if(!checkOrphanParameters()) throw Exception("DEBUG: SubstitutionModelSet::unsetParameterToModel. Orphan parameterl!");
    }

    /**
     * @brief Add a parameter to the list, and link it to specified existing nodes.
     *
     * @param parameter The parameter to add. Its name must match model parameter names.
     * @param nodesId The list of ids of the nodes to link with these parameters.
     * Nodes must have a corresponding model in the set.
     * @throw Exception If one of the above requirement is not true.
     */
    void addParameter(const Parameter & parameter, const vector<int> & nodesId) throw (Exception);
 
    /**
     * @brief Add several parameters to the list, and link them to specified existing nodes.
     *
     * @param parameters The list of parameters to add. Its name must match model parameter names.
     * @param nodesId The list of ids of the nodes to link with these parameters.
     * Nodes must have a corresponding model in the set.
     * @throw Exception If one of the above requirement is not true.
     */
    void addParameters(const ParameterList & parameters, const vector<int> & nodesId) throw (Exception);
 
    /**
     * @brief Remove a model from the set, and all corresponding parameters.
     *
     * @param modelIndex The index of the model in the set.
     * @throw Exception if a parameter becomes orphan because of the removal.
     */
    void removeModel(unsigned int modelIndex) throw (Exception);

    void listModelNames(ostream & out = cout) const;

    //void setRootFrequencies(const vector<double> & initFreqs) throw (Exception);

    vector<double> getRootFrequencies() const { return _rootFrequencies->getFrequencies(); }
    
    /**
     * @brief Get the parameters corresponding to the root frequencies.
     *
     * This corresponds to the [number of states] parameters in the list.
     *
     * @return The parameters corresponding to the root frequencies.
     */
    ParameterList getRootFrequenciesParameters() const
    { 
      return _rootFrequencies->getParameters();
    }

    const Alphabet * getAlphabet() const { return _alphabet; }

    /**
     * @brief Check if the model set is fully specified for a given tree.
     *
     * This include:
     * - that each node as a model set up,
     * - that each model in the set is attributed to a node,
     * - that each parameter in the set actually correspond to a model.
     *
     * @param tree The tree to check.
     */
    bool isFullySetUpFor(const Tree & tree) const
    {
      return checkOrphanModels()
          && checkOrphanParameters()
          && checkOrphanNodes(tree);
    }

  protected:

    /**
     * Set _rootFrequencies from _parameters.
     */
    void updateRootFrequencies()
    {
      _rootFrequencies->matchParametersValues(_parameters);
    }

    /**
     * @name Chek function.
     *
     * @{
     */
    bool checkOrphanModels() const;

    bool checkOrphanParameters() const;

    bool checkOrphanNodes(const Tree & tree) const;
    /** @} */

};

#endif // _SUBSTITUTIONMODELSET_H_
