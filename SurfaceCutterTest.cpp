#include <vtkArrayDispatch.h>
#include <vtkCellIterator.h>
#include <vtkDataArray.h>
#include <vtkDataArrayAccessor.h>
#include <vtkIdList.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkXMLPolyDataReader.h>
#include "SurfaceCutter.h"

#include <array>
#include <string>
#include <sstream>

// functor to compare mixed precision point coords.
struct PointComparator {
  template<typename PointsArrT1, typename PointsArrT2>
  void operator()(PointsArrT1* points1, PointsArrT2* points2)
  {
    const vtkIdType& numPts1 = points1->GetNumberOfTuples();
    const vtkIdType& numPts2 = points2->GetNumberOfTuples();
    
    assert(numPts1 == numPts2);

    using Points1Acc = vtkDataArrayAccessor<PointsArrT1>;
    using Points2Acc = vtkDataArrayAccessor<PointsArrT2>;

    using Points1T = Points1Acc::APIType;
    using Points2T = Points2Acc::APIType;

    VTK_ASSUME(points1->GetNumberOfComponents() == 3);
    VTK_ASSUME(points2->GetNumberOfComponents() == 3);

    Points1Acc points1Acc(points1);
    Points2Acc points2Acc(points2);

    const vtkIdType& numTuples = numPts1;

    for (vtkIdType tup = 0; tup < numTuples; ++tup)
    {
      std::array<Points1T, 3> p1;
      points1Acc.Get(tup, p1.data());
      
      std::array<Points2T, 3> p2;
      points2Acc.Get(tup, p2.data());

      if (sizeof(Points1T) <= sizeof(Points2T))
      {
        constexpr auto eps = std::numeric_limits<Points1T>::epsilon();
        for (unsigned short iDim = 0; iDim < 3; ++iDim)
        {
          assert(p1[iDim] - p2[iDim] <= eps);
        }
      }
      else
      {
        constexpr auto eps = std::numeric_limits<Points2T>::epsilon();
        for (unsigned short iDim = 0; iDim < 3; ++iDim)
        {
          assert(p1[iDim] - p2[iDim] <= eps);
        }
      }
    }
  }
};

int test_case(const unsigned short& caseIdx, vtkSmartPointer<SurfaceCutter> surfCutter, const bool& insideOut)
{
  std::stringstream caseFname;
  caseFname << "data/Case" << caseIdx << ".vtp";

  std::stringstream testFname;
  testFname << "data/TestCase" << caseIdx;
  if (insideOut)
    testFname << "InOutTrue.vtp";
  else
    testFname << "InOutFalse.vtp";

  auto caseReader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
  caseReader->SetFileName(caseFname.str().c_str());

  surfCutter->SetInputConnection(1, caseReader->GetOutputPort());
  surfCutter->SetInsideOut(true);
  surfCutter->Update();

  auto surfCut = vtkSmartPointer<vtkPolyData>::New();
  surfCut->ShallowCopy(surfCutter->GetOutput());

  auto testSurfCut = vtkSmartPointer<vtkPolyData>::New();
  auto testCaseReader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
  testCaseReader->SetFileName(testFname.str().c_str());
  testCaseReader->Update();
  testSurfCut->ShallowCopy(testCaseReader->GetOutput());

  using dispatcher = vtkArrayDispatch::Dispatch2ByValueType<vtkArrayDispatch::Reals, vtkArrayDispatch::Reals>;

  if (!(surfCut->GetPoints() || testSurfCut->GetPoints()))
  {
    std::cerr << "Unexpected error! Empty point set." << std::endl;
    return EXIT_FAILURE;
  }

  vtkDataArray* points1 = surfCut->GetPoints()->GetData();
  vtkDataArray* points2 = testSurfCut->GetPoints()->GetData();
  
  auto comparator = PointComparator();
  if (!dispatcher::Execute(points1, points2, comparator))
    comparator(points1, points2);

  const vtkIdType& numTris = surfCut->GetNumberOfPolys();
  assert(numTris == testSurfCut->GetNumberOfCells());

  vtkSmartPointer<vtkCellIterator> iter = surfCut->NewCellIterator();
  vtkSmartPointer<vtkCellIterator> testIter = testSurfCut->NewCellIterator();
  for (iter->InitTraversal(), testIter->InitTraversal();
    !iter->IsDoneWithTraversal(), !testIter->IsDoneWithTraversal();
    iter->GoToNextCell(), testIter->GoToNextCell())
  {
    // same cell type
    const int& cellType = iter->GetCellType();
    const int& testCellType = testIter->GetCellType();
    assert(cellType == testCellType);

    // same pt ids.
    vtkSmartPointer<vtkIdList> ptIds = iter->GetPointIds();
    vtkSmartPointer<vtkIdList> testPtIds = testIter->GetPointIds();
    const vtkIdType& numPtIds = ptIds->GetNumberOfIds();
    const vtkIdType& numTestPtIds = testPtIds->GetNumberOfIds();
    assert(numPtIds == numTestPtIds);
    for (vtkIdType iPtId = 0; iPtId < numPtIds; ++iPtId)
      assert(ptIds->GetId(iPtId) == testPtIds->GetId(iPtId));
  }

  return EXIT_SUCCESS;
}

int main()
{
  auto reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
  reader->SetFileName("data/Triangle.vtp");
  auto surfCutter = vtkSmartPointer<SurfaceCutter>::New();
  surfCutter->SetInputConnection(0, reader->GetOutputPort());
  for (unsigned short iCase = 1; iCase < 6; ++iCase)
  {
    if (!test_case(iCase, surfCutter, true))
      return EXIT_FAILURE;
    if (!test_case(iCase, surfCutter, false))
      return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}