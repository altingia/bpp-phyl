//
// File: AbstractTreeDrawing.h
// Created by: Julien Dutheil
// Created on: Sun Oct 8 11:57 2006
//

/*
Copyright or � or Copr. CNRS, (November 16, 2004)

This software is a computer program whose purpose is to provide
graphic components to develop bioinformatics applications.

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

#ifndef _ABSTRACTTREEDRAWING_H_
#define _ABSTRACTTREEDRAWING_H_

#include "TreeTemplate.h"
#include "NodeTemplate.h"
#include "TreeDrawing.h"

namespace bpp
{

class TreeDrawingNodeInfo
{
  private:
    Point2D<double> pos_;
    
  public:
    TreeDrawingNodeInfo() {}
    virtual ~TreeDrawingNodeInfo() {}
        
  public:
    const Point2D<double>& getPosition() const { return pos_; }
    Point2D<double>& getPosition() { return pos_; }
    void setPosition(const Point2D<double>& position) { pos_ = position; }
    double getX() const { return pos_.getX(); }
    double getY() const { return pos_.getY(); }
    void setX(double x) { pos_.setX(x); }
    void setY(double y) { pos_.setY(y); }
};

typedef NodeTemplate<TreeDrawingNodeInfo> INode;

/**
 * @brief Partial implementation of the TreeDrawing interface.
 *
 * This basic implementation uses a dedicated NodeInfo structure in combination with the NodeTemplate class.
 * This structures stores the current coordinates of all nodes, so that it is easy to annotate the tree drawing.:if expand("%") == ""|browse confirm w|else|confirm w|endif
 *
 */
class AbstractTreeDrawing:
  public virtual TreeDrawing
{
  private:
    TreeTemplate<INode> *tree_;
    double xUnit_;
    double yUnit_;
    double pointArea_;
    TreeDrawingSettings settings_;
 
  public:
    AbstractTreeDrawing(const Tree* tree = 0);
    
    AbstractTreeDrawing(const AbstractTreeDrawing& atd)
    {
      tree_       = atd.tree_->clone();
      xUnit_      = atd.xUnit_;
      yUnit_      = atd.yUnit_;
      settings_   = atd.settings_;
    }
     
    AbstractTreeDrawing& operator=(const AbstractTreeDrawing& atd)
    {
      tree_       = atd.tree_->clone();
      xUnit_      = atd.xUnit_;
      yUnit_      = atd.yUnit_;
      settings_   = atd.settings_;
      return *this;
    }

    virtual ~AbstractTreeDrawing()
    {
      if (tree_) delete tree_;   
    }
  
  public:
    
    bool hasTree() const { return tree_ != 0; }

#ifndef NO_VIRTUAL_COV
    Tree*
#else
    const TreeTemplate<INOde>*
#endif
    getTree() const { return tree_; }
    
    void setTree(const Tree* tree)
    {
      if (tree_) delete tree_;
      if (!tree) tree_ = 0;
      else
      {
        tree_ = new TreeTemplate<INode>(*tree); //We copy the tree
      }
    }
    
    Point2D<double> getNodePosition(int nodeId) const throw (NodeNotFoundException);

    int getNodeAt(const Point2D<double>& position) const throw (NodeNotFoundException);
 
    /**
     * @brief Utilitary function, telling if a point belongs to a specified area.
     *
     * This method is used internally to get a node coordinates.
     *
     * @param p1 Point to look for.
     * @param p2 Second point defining the center of the area.
     * @return True if p1 belongs to the area defined by p2.
     */
    bool belongsTo(const Point2D<double>& p1, const Point2D<double>& p2) const;

    /**
     * @brief Draw some text at a particular node position.
     *
     * @param gDevice The GraphicDevice object on which to draw.
     * @param node The node of interest.
     * @param text The text to draw.
     * @param xOffset Horizontal offset.
     * @param yOffset Vertical offset.
     * @param hpos The way the text should be aligned horizontally (see GraphicDevice).
     * @param vpos The way the text should be aligned vertically (see GraphicDevice).
     * @param angle The rotation value of the text.
     */
    virtual void drawAtNode(GraphicDevice& gDevice, const INode& node, const string& text, double xOffset = 0, double yOffset = 0, short hpos = GraphicDevice::TEXT_HORIZONTAL_LEFT, short vpos = GraphicDevice::TEXT_VERTICAL_CENTER, double angle = 0) const;

    /**
     * @brief Draw some text at a particular branch position.
     *
     * @param gDevice The GraphicDevice object on which to draw.
     * @param node The node of interest.
     * @param text The text to draw.
     * @param xOffset Horizontal offset.
     * @param yOffset Vertical offset.
     * @param hpos The way the text should be aligned horizontally (see GraphicDevice).
     * @param vpos The way the text should be aligned vertically (see GraphicDevice).
     * @param angle The rotation value of the text.
      */
    virtual void drawAtBranch(GraphicDevice& gDevice, const INode& node, const string& text, double xOffset = 0, double yOffset = 0, short hpos = GraphicDevice::TEXT_HORIZONTAL_LEFT, short vpos = GraphicDevice::TEXT_VERTICAL_CENTER, double angle = 0) const;
   
    void setDisplaySettings(TreeDrawingSettings& tds) { settings_ = tds; }
    TreeDrawingSettings & getDisplaySettings() { return settings_; }
    const TreeDrawingSettings & getDisplaySettings() const { return settings_; }

    double getXUnit() const { return xUnit_; }
    
    double getYUnit() const { return yUnit_; }

    void setXUnit(double xu) { xUnit_ = xu; }
    
    void setYUnit(double yu) { yUnit_ = yu; }

  protected:
    TreeTemplate<INode>* getTree_() { return tree_; }
    const TreeTemplate<INode>* getTree_() const { return tree_; }
 
};

} //end of namespace bpp.

#endif //_ABSTRACTTREEDRAWING_H_
