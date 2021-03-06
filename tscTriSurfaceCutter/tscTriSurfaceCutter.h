#pragma once
/**
 *
 * @file  tscTriSurfaceCutter.h
 * @class tscTriSurfaceCutter
 * @brief Cut a triangulated surface with one or more polygons.
 *
 * This filter is geometrically based, unlike vtkClipDataSet and vtkClipPolyData
 * (both of which are scalar-based).
 *             
 * It crops an input vtkPolyData consisting of triangles
 * with loops specified by a second input containing polygons.
 *             
 * The loop polygons can be concave, can have vertices exactly 
 * coincident with a mesh point/edge.
 * 
 * It computes an **embedding** of the loop polygons' edges upon the mesh 
 * followed by **removal** of triangles *in(out)side the polygons. See SetInsideOut().
 * 
 * Linear cells other than triangles will be passed through.
 * Line segments and polylines from input will be marked as constraints.
 * 
 * It is possible to output a pure embedding or a pure removal.
 *             
 * @note:
 * Input point-data is interpolated to output.
 * Input cell-data is copied to output.
 *
 * @sa
 * vtkClipDataSet vtkClipPolyData
 *
 */

#include <tscTriSurfaceCutterModule.h>

#include <vector>

#include "vtkAbstractCellLocator.h"
#include "vtkGenericCell.h"
#include "vtkIncrementalPointLocator.h"
#include "vtkPolyDataAlgorithm.h"
#include "vtkSmartPointer.h"
#include "vtkTriangle.h"

class vtkPolyData;

class TSCTRISURFACECUTTER_EXPORT tscTriSurfaceCutter : public vtkPolyDataAlgorithm {
public:
  /**
   * Construct object with tolerance 1.0e-6, inside out set to true,
   * color acquired points, color loop edges
   */
  static tscTriSurfaceCutter *New();
  vtkTypeMacro(tscTriSurfaceCutter, vtkPolyDataAlgorithm);
  void PrintSelf(ostream &os, vtkIndent indent) override;

  //@{
  /**
   * Accelerate cell searches with a multi-threaded cell locator. Default: On
   */
  vtkBooleanMacro(AccelerateCellLocator, bool);
  vtkSetMacro(AccelerateCellLocator, bool);
  vtkGetMacro(AccelerateCellLocator, bool);
  //@}

  //@{
  /**
   * After the loop's edges are embedded onto the surface,
   * On: remove stuff outside all loop polygons
   * Off: remove stuff inside atleast one loop polygon
   */
  vtkBooleanMacro(InsideOut, bool);
  vtkSetMacro(InsideOut, bool);
  vtkGetMacro(InsideOut, bool);
  //@}

  //@{
  /**
   * Numeric tolerance for point merging, intersection math.
   */
  vtkSetMacro(Tolerance, double);
  vtkGetMacro(Tolerance, double);
  //@}

  //@{
  /**
   * Specify a subclass of vtkAbstractCellLocator which implements the method
   * 'FindCellsWithinBounds()'. Ex: vtkStaticCellLocator, vtkCellLocator. Not
   * vtkOBBTree
   */
  vtkSetSmartPointerMacro(CellLocator, vtkAbstractCellLocator);
  vtkGetSmartPointerMacro(CellLocator, vtkAbstractCellLocator);
  //@}

  //@{
  /**
   * Specify a spatial point locator for merging points. By default, an
   * instance of vtkMergePoints is used.
   */
  vtkSetSmartPointerMacro(PointLocator, vtkIncrementalPointLocator);
  vtkGetSmartPointerMacro(PointLocator, vtkIncrementalPointLocator);
  //@}

  //@{
  /**
   * Do not respect the very functionality of this filter. Only embed loop
   * polygons onto the mesh
   * @note InsideOut option does not apply here.
   */
  vtkBooleanMacro(Embed, bool);
  vtkSetMacro(Embed, bool);
  vtkGetMacro(Embed, bool);
  //@}

  //@{
  /**
   * Partially respect functionality of this filter. Only remove cells
   * in(out)side loop polygons.
   */
  vtkBooleanMacro(Remove, bool);
  vtkSetMacro(Remove, bool);
  vtkGetMacro(Remove, bool);
  //@}

  /**
   * Specify the a second vtkPolyData input which defines loops used to cut
   * the input polygonal data. These loops must be manifold, i.e., do not
   * self intersect. The loops are defined from the polygons defined in
   * this second input.
   */
  void SetLoopsData(vtkPolyData *loops);

  /**
   * Specify the a second vtkPolyData input which defines loops used to cut
   * the input polygonal data. These loops must be manifold, i.e., do not
   * self intersect. The loops are defined from the polygons defined in
   * this second input.
   */
  void SetLoopsConnection(vtkAlgorithmOutput *output);

  /**
   * Create default locators. Used to create one when none are specified.
   * The point locator is used to merge coincident points.
   * The cell locator is used to accelerate cell searches.
   */
  void CreateDefaultLocators();

protected:
  tscTriSurfaceCutter();
  ~tscTriSurfaceCutter() override;

  bool AccelerateCellLocator;
  bool Embed;
  bool InsideOut;
  bool Remove;
  double Tolerance;

  vtkSmartPointer<vtkAbstractCellLocator> CellLocator;
  vtkSmartPointer<vtkIncrementalPointLocator> PointLocator;

  int RequestData(vtkInformation *request, vtkInformationVector **inputVector,
                  vtkInformationVector *outputVector) override;
  int FillInputPortInformation(int vtkNotUsed(port), vtkInformation *info) override;
  int FillOutputPortInformation(int vtkNotUsed(port), vtkInformation *info) override;

private:
  tscTriSurfaceCutter(const tscTriSurfaceCutter &) = delete;
  void operator=(const tscTriSurfaceCutter &) = delete;
};

namespace tsc_detail
{
  // Convention: in a triangle - v: vertex, e: edge
  // with vertices v0--v1--v2,
  // e0 = v0--v1, e1 = v1--v2, e2 = v2--v0;
  const std::array<std::pair<vtkIdType, vtkIdType>, 3> TRIEDGES = { std::make_pair(0, 1),
    std::make_pair(1, 2), std::make_pair(2, 0) };
  const vtkIdType TRIOPPEDGE[3] = { 1, 2, 0 };  // edge id opposite to a vertex
  const vtkIdType TRIOPPVERTS[3] = { 2, 0, 1 }; // vertex id opposite to an edge

  enum class PointInTriangle
  {
    OnVertex,
    OnEdge,
    Inside,
    Outside,
    Degenerate
  };

  enum class PointOnLine
  {
    OnVertex,
    Inside,
    Outside,
  };

  enum class IntersectType
  {
    NoIntersection = 0,
    Intersect,
    Junction
  };
  // A child is born when a parent triangle/line crosses a loop's edge.
  struct Child
  {
    /**
     * @brief When a triangle(line) is cut by a loop polygon, it
     *        births children triangles(lines)
     */
    Child();
    Child(const double& cx_, const double& cy_, const vtkIdType* pts, const vtkIdType npts_,
      const double bounds[4]);
    Child(const double& cx_, const double& cy_, vtkIdList* pts, const double bounds[4]);

    double cx, cy;               // centroid
    vtkNew<vtkIdList> point_ids; // the ids
    vtkBoundingBox bbox;         // bbox of all points
  };

  class Parent : public vtkGenericCell
  {
  public:
    static Parent* New();
    vtkTypeMacro(Parent, vtkGenericCell);
    void Reset();
    void UpdateChildren(vtkCellArray* polys);

    std::vector<Child> children;
    vtkIdType cellId;

  protected:
    /**
     * @brief When a root triangle(line) intersects a line segment, it births child
     * triangles(lines).
     */
    Parent();
    ~Parent() override;

  private:
    Parent(const Parent&) = delete;
    void operator=(const Parent&) = delete;
  };
}
