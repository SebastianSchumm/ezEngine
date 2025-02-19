#include <JoltCooking/JoltCookingPCH.h>

#include <Core/Graphics/ConvexHull.h>
#include <Foundation/Configuration/Singleton.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/ChunkStream.h>
#include <Foundation/Math/BoundingBoxSphere.h>
#include <Foundation/Time/Stopwatch.h>
#include <Foundation/Utilities/Progress.h>
#include <JoltCooking/JoltCooking.h>

#include <Foundation/IO/MemoryStream.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Math/Float3.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <JoltPlugin/Utilities/JoltConversionUtils.h>
#include <Physics/Collision/Shape/ConvexHullShape.h>

#define ENABLE_VHACD_IMPLEMENTATION 1
#include <VHACD/VHACD.h>
using namespace VHACD;

class ezJoltStreamOut : public JPH::StreamOut
{
public:
  ezJoltStreamOut(ezStreamWriter* pPassThrough)
  {
    m_pWriter = pPassThrough;
  }

  virtual void WriteBytes(const void* inData, size_t inNumBytes) override
  {
    if (m_pWriter->WriteBytes(inData, inNumBytes).Failed())
      m_bFailed = true;
  }

  virtual bool IsFailed() const override
  {
    return m_bFailed;
  }


private:
  ezStreamWriter* m_pWriter = nullptr;
  bool m_bFailed = false;
};

ezResult ezJoltCooking::CookTriangleMesh(const ezJoltCookingMesh& mesh, ezStreamWriter& OutputStream)
{
  JPH::VertexList vertexList;
  JPH::IndexedTriangleList triangleList;

  // copy vertices
  {
    vertexList.resize(mesh.m_Vertices.GetCount());
    for (ezUInt32 i = 0; i < mesh.m_Vertices.GetCount(); ++i)
    {
      vertexList[i] = ezJoltConversionUtils::ToFloat3(mesh.m_Vertices[i]);
    }
  }

  ezUInt32 uiMaxMaterialIndex = 0;

  // compute number of triangles
  {
    ezUInt32 uiTriangles = 0;

    for (ezUInt32 i = 0; i < mesh.m_VerticesInPolygon.GetCount(); ++i)
    {
      uiTriangles += mesh.m_VerticesInPolygon[i] - 2;
    }

    triangleList.resize(uiTriangles);
  }

  // triangulate
  {
    ezUInt32 uiIdxOffset = 0;
    ezUInt32 uiTriIdx = 0;

    for (ezUInt32 poly = 0; poly < mesh.m_VerticesInPolygon.GetCount(); ++poly)
    {
      const ezUInt32 polyVerts = mesh.m_VerticesInPolygon[poly];

      for (ezUInt32 tri = 0; tri < polyVerts - 2; ++tri)
      {
        const ezUInt32 uiMaterialID = mesh.m_PolygonSurfaceID[poly];

        uiMaxMaterialIndex = ezMath::Max(uiMaxMaterialIndex, uiMaterialID);

        const ezUInt32 idx0 = mesh.m_PolygonIndices[uiIdxOffset + 0];
        const ezUInt32 idx1 = mesh.m_PolygonIndices[uiIdxOffset + tri + 1];
        const ezUInt32 idx2 = mesh.m_PolygonIndices[uiIdxOffset + tri + 2];

        triangleList[uiTriIdx].mMaterialIndex = uiMaterialID;
        triangleList[uiTriIdx].mIdx[0] = idx0;
        triangleList[uiTriIdx].mIdx[1] = idx1;
        triangleList[uiTriIdx].mIdx[2] = idx2;

        ++uiTriIdx;
      }

      uiIdxOffset += polyVerts;
    }
  }

  // cook mesh (create Jolt shape, then save to binary stream)
  {
    JPH::MeshShapeSettings meshSettings(vertexList, triangleList);
    meshSettings.mMaterials.resize(uiMaxMaterialIndex + 1);

    auto shapeRes = meshSettings.Create();

    if (shapeRes.HasError())
    {
      ezLog::Error("Cooking Jolt triangle mesh failed: {}", shapeRes.GetError().c_str());
      return EZ_FAILURE;
    }

    ezDefaultMemoryStreamStorage storage;
    ezMemoryStreamWriter memWriter(&storage);

    ezJoltStreamOut jOut(&memWriter);
    shapeRes.Get()->SaveBinaryState(jOut);

    OutputStream << storage.GetStorageSize32();
    storage.CopyToStream(OutputStream).AssertSuccess();

    const ezUInt32 uiNumVertices = static_cast<ezUInt32>(vertexList.size());
    OutputStream << uiNumVertices;

    const ezUInt32 uiNumTriangles = shapeRes.Get()->GetStats().mNumTriangles;
    OutputStream << uiNumTriangles;
  }

  return EZ_SUCCESS;
}

ezResult ezJoltCooking::CookConvexMesh(const ezJoltCookingMesh& mesh0, ezStreamWriter& OutputStream)
{
  ezProgressRange range("Cooking Convex Mesh", 2, false);

  range.BeginNextStep("Computing Convex Hull");

  ezJoltCookingMesh mesh;
  EZ_SUCCEED_OR_RETURN(ComputeConvexHull(mesh0, mesh));

  range.BeginNextStep("Cooking Convex Hull");

  EZ_SUCCEED_OR_RETURN(CookSingleConvexJoltMesh(mesh, OutputStream));

  return EZ_SUCCESS;
}

EZ_DEFINE_AS_POD_TYPE(JPH::Vec3);

ezResult ezJoltCooking::CookSingleConvexJoltMesh(const ezJoltCookingMesh& mesh, ezStreamWriter& OutputStream)
{
  ezHybridArray<JPH::Vec3, 256> verts;
  verts.SetCountUninitialized(mesh.m_Vertices.GetCount());

  for (ezUInt32 i = 0; i < verts.GetCount(); ++i)
  {
    ezVec3 v = mesh.m_Vertices[i];
    verts[i] = JPH::Vec3(v.x, v.y, v.z);
  }

  JPH::ConvexHullShapeSettings shapeSettings(verts.GetData(), (int)verts.GetCount());

  auto shapeRes = shapeSettings.Create();

  if (shapeRes.HasError())
  {
    ezLog::Error("Cooking convex Jolt mesh failed: {}", shapeRes.GetError().c_str());
    return EZ_FAILURE;
  }

  ezDefaultMemoryStreamStorage storage;
  ezMemoryStreamWriter memWriter(&storage);

  ezJoltStreamOut jOut(&memWriter);
  shapeRes.Get()->SaveBinaryState(jOut);

  OutputStream << storage.GetStorageSize32();
  storage.CopyToStream(OutputStream).AssertSuccess();

  const ezUInt32 uiNumVertices = verts.GetCount();
  OutputStream << uiNumVertices;

  const ezUInt32 uiNumTriangles = shapeRes.Get()->GetStats().mNumTriangles;
  OutputStream << uiNumTriangles;

  return EZ_SUCCESS;
}

ezResult ezJoltCooking::ComputeConvexHull(const ezJoltCookingMesh& mesh, ezJoltCookingMesh& outMesh)
{
  ezStopwatch timer;

  outMesh.m_bFlipNormals = mesh.m_bFlipNormals;


  ezConvexHullGenerator gen;
  if (gen.Build(mesh.m_Vertices).Failed())
  {
    ezLog::Error("Computing the convex hull failed.");
    return EZ_FAILURE;
  }

  ezDynamicArray<ezConvexHullGenerator::Face> faces;
  gen.Retrieve(outMesh.m_Vertices, faces);

  if (faces.GetCount() >= 255)
  {
    ezConvexHullGenerator gen2;
    gen2.SetSimplificationMinTriangleAngle(ezAngle::Degree(30));
    gen2.SetSimplificationFlatVertexNormalThreshold(ezAngle::Degree(10));
    gen2.SetSimplificationMinTriangleEdgeLength(0.08f);

    if (gen2.Build(outMesh.m_Vertices).Failed())
    {
      ezLog::Error("Computing the convex hull failed (second try).");
      return EZ_FAILURE;
    }

    gen2.Retrieve(outMesh.m_Vertices, faces);
  }


  for (const auto& face : faces)
  {
    outMesh.m_VerticesInPolygon.ExpandAndGetRef() = 3;
    outMesh.m_PolygonSurfaceID.ExpandAndGetRef() = 0;

    for (int vert = 0; vert < 3; ++vert)
      outMesh.m_PolygonIndices.ExpandAndGetRef() = face.m_uiVertexIdx[vert];
  }

  ezLog::Dev("Computed the convex hull in {0} milliseconds", ezArgF(timer.GetRunningTotal().GetMilliseconds(), 1));
  return EZ_SUCCESS;
}

ezStatus ezJoltCooking::WriteResourceToStream(ezChunkStreamWriter& stream, const ezJoltCookingMesh& mesh, const ezArrayPtr<ezString>& surfaces, MeshType meshType, ezUInt32 uiMaxConvexPieces)
{
  ezResult resCooking = EZ_FAILURE;

  {
    stream.BeginChunk("Surfaces", 1);

    stream << surfaces.GetCount();

    for (const auto& slot : surfaces)
    {
      stream << slot;
    }

    stream.EndChunk();
  }

  {
    stream.BeginChunk("Details", 1);

    ezBoundingBoxSphere aabb;
    aabb.SetFromPoints(mesh.m_Vertices.GetData(), mesh.m_Vertices.GetCount());

    stream << aabb;

    stream.EndChunk();
  }

  if (meshType == MeshType::Triangle)
  {
    stream.BeginChunk("TriangleMesh", 1);

    ezStopwatch timer;
    resCooking = ezJoltCooking::CookTriangleMesh(mesh, stream);
    ezLog::Dev("Triangle Mesh Cooking time: {0}s", ezArgF(timer.GetRunningTotal().GetSeconds(), 2));

    stream.EndChunk();
  }
  else
  {
    if (meshType == MeshType::ConvexDecomposition)
    {
      stream.BeginChunk("ConvexDecompositionMesh", 1);

      ezStopwatch timer;
      resCooking = ezJoltCooking::CookDecomposedConvexMesh(mesh, stream, uiMaxConvexPieces);
      ezLog::Dev("Decomposed Convex Mesh Cooking time: {0}s", ezArgF(timer.GetRunningTotal().GetSeconds(), 2));

      stream.EndChunk();
    }

    if (meshType == MeshType::ConvexHull)
    {
      stream.BeginChunk("ConvexMesh", 1);

      ezStopwatch timer;
      resCooking = ezJoltCooking::CookConvexMesh(mesh, stream);
      ezLog::Dev("Convex Mesh Cooking time: {0}s", ezArgF(timer.GetRunningTotal().GetSeconds(), 2));

      stream.EndChunk();
    }
  }

  if (resCooking.Failed())
    return ezStatus("Cooking the collision mesh failed.");


  return ezStatus(EZ_SUCCESS);
}

ezResult ezJoltCooking::CookDecomposedConvexMesh(const ezJoltCookingMesh& mesh, ezStreamWriter& OutputStream, ezUInt32 uiMaxConvexPieces)
{
  EZ_LOG_BLOCK("Decomposing Mesh");

  IVHACD* pConDec = CreateVHACD();
  IVHACD::Parameters params;
  params.m_maxConvexHulls = ezMath::Max(1u, uiMaxConvexPieces);

  if (uiMaxConvexPieces <= 2)
  {
    params.m_resolution = 10 * 10 * 10;
  }
  else if (uiMaxConvexPieces <= 5)
  {
    params.m_resolution = 20 * 20 * 20;
  }
  else if (uiMaxConvexPieces <= 10)
  {
    params.m_resolution = 40 * 40 * 40;
  }
  else if (uiMaxConvexPieces <= 25)
  {
    params.m_resolution = 60 * 60 * 60;
  }
  else if (uiMaxConvexPieces <= 50)
  {
    params.m_resolution = 80 * 80 * 80;
  }
  else
  {
    params.m_resolution = 100 * 100 * 100;
  }

  if (!pConDec->Compute(mesh.m_Vertices.GetData()->GetData(), mesh.m_Vertices.GetCount(), mesh.m_PolygonIndices.GetData(), mesh.m_VerticesInPolygon.GetCount(), params))
  {
    ezLog::Error("Failed to compute convex decomposition");
    return EZ_FAILURE;
  }

  ezUInt16 uiNumParts = 0;

  for (ezUInt32 i = 0; i < pConDec->GetNConvexHulls(); ++i)
  {
    IVHACD::ConvexHull ch;
    pConDec->GetConvexHull(i, ch);

    if (ch.m_triangles.empty())
      continue;

    ++uiNumParts;
  }

  ezLog::Dev("Convex mesh parts: {}", uiNumParts);

  OutputStream << uiNumParts;

  for (ezUInt32 i = 0; i < pConDec->GetNConvexHulls(); ++i)
  {
    IVHACD::ConvexHull ch;
    pConDec->GetConvexHull(i, ch);

    if (ch.m_triangles.empty())
      continue;

    ezJoltCookingMesh chm;

    chm.m_Vertices.SetCount((ezUInt32)ch.m_points.size());

    for (ezUInt32 v = 0; v < (ezUInt32)ch.m_points.size(); ++v)
    {
      chm.m_Vertices[v].Set((float)ch.m_points[v].mX, (float)ch.m_points[v].mY, (float)ch.m_points[v].mZ);
    }

    chm.m_VerticesInPolygon.SetCount((ezUInt32)ch.m_triangles.size());
    chm.m_PolygonSurfaceID.SetCount((ezUInt32)ch.m_triangles.size());
    chm.m_PolygonIndices.SetCount((ezUInt32)ch.m_triangles.size() * 3);

    for (ezUInt32 t = 0; t < (ezUInt32)ch.m_triangles.size(); ++t)
    {
      chm.m_VerticesInPolygon[t] = 3;
      chm.m_PolygonSurfaceID[t] = 0;

      chm.m_PolygonIndices[t * 3 + 0] = ch.m_triangles[t].mI0;
      chm.m_PolygonIndices[t * 3 + 1] = ch.m_triangles[t].mI1;
      chm.m_PolygonIndices[t * 3 + 2] = ch.m_triangles[t].mI2;
    }

    EZ_SUCCEED_OR_RETURN(CookSingleConvexJoltMesh(chm, OutputStream));
  }

  return EZ_SUCCESS;
}

EZ_STATICLINK_FILE(JoltCooking, JoltCooking_JoltCooking);
