#pragma once

#include <EditorFramework/Assets/SimpleAssetDocument.h>
#include <ToolsFoundation/Object/DocumentObjectBase.h>

struct ezPropertyMetaStateEvent;

struct ezJoltSurfaceResourceSlot
{
  ezString m_sLabel;
  ezString m_sResource;
};

struct ezJoltCollisionMeshType
{
  typedef ezInt8 StorageType;

  enum Enum
  {
    ConvexHull,
    TriangleMesh,
    Cylinder,

    Default = TriangleMesh
  };
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_NO_LINKAGE, ezJoltCollisionMeshType);

struct ezJoltConvexCollisionMeshType
{
  typedef ezInt8 StorageType;

  enum Enum
  {
    ConvexHull,
    Cylinder,
    ConvexDecomposition,

    Default = ConvexHull
  };
};

EZ_DECLARE_REFLECTABLE_TYPE(EZ_NO_LINKAGE, ezJoltConvexCollisionMeshType);

EZ_DECLARE_REFLECTABLE_TYPE(EZ_NO_LINKAGE, ezJoltSurfaceResourceSlot);

class ezJoltCollisionMeshAssetProperties : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezJoltCollisionMeshAssetProperties, ezReflectedClass);

public:
  ezJoltCollisionMeshAssetProperties();
  ~ezJoltCollisionMeshAssetProperties();

  static void PropertyMetaStateEventHandler(ezPropertyMetaStateEvent& e);

  ezString m_sMeshFile;
  float m_fUniformScaling = 1.0f;

  ezEnum<ezBasisAxis> m_RightDir = ezBasisAxis::PositiveX;
  ezEnum<ezBasisAxis> m_UpDir = ezBasisAxis::PositiveY;
  bool m_bFlipForwardDir = false;
  bool m_bIsConvexMesh = false;
  ezEnum<ezJoltConvexCollisionMeshType> m_ConvexMeshType;
  ezUInt16 m_uiMaxConvexPieces = 2;

  // Cylinder
  float m_fRadius = 0.5f;
  float m_fRadius2 = 0.5f;
  float m_fHeight = 1.0f;
  ezUInt8 m_uiDetail = 1;

  ezHybridArray<ezJoltSurfaceResourceSlot, 8> m_Slots;

  ezUInt32 m_uiVertices = 0;
  ezUInt32 m_uiTriangles = 0;
};
