
#pragma once

#include <Foundation/Basics.h>
#include <RendererFoundation/RendererFoundationDLL.h>

// \brief This class can be used to define the render targets to be used by an ezView.
struct EZ_RENDERERFOUNDATION_DLL ezGALRenderTargets
{
  bool operator==(const ezGALRenderTargets& other) const;
  bool operator!=(const ezGALRenderTargets& other) const;

  ezGALTextureHandle m_hRTs[EZ_GAL_MAX_RENDERTARGET_COUNT];
  ezGALTextureHandle m_hDSTarget;
};

// \brief This class can be used to construct render target setups on the stack.
class EZ_RENDERERFOUNDATION_DLL ezGALRenderTargetSetup
{
public:
  ezGALRenderTargetSetup();

  ezGALRenderTargetSetup& SetRenderTarget(ezUInt8 uiIndex, ezGALRenderTargetViewHandle hRenderTarget);
  ezGALRenderTargetSetup& SetDepthStencilTarget(ezGALRenderTargetViewHandle hDSTarget);

  bool operator==(const ezGALRenderTargetSetup& other) const;
  bool operator!=(const ezGALRenderTargetSetup& other) const;

  inline ezUInt8 GetRenderTargetCount() const;

  inline ezGALRenderTargetViewHandle GetRenderTarget(ezUInt8 uiIndex) const;
  inline ezGALRenderTargetViewHandle GetDepthStencilTarget() const;

  void DestroyAllAttachedViews();

protected:
  ezGALRenderTargetViewHandle m_hRTs[EZ_GAL_MAX_RENDERTARGET_COUNT];
  ezGALRenderTargetViewHandle m_hDSTarget;

  ezUInt8 m_uiRTCount = 0;
};

struct EZ_RENDERERFOUNDATION_DLL ezGALRenderingSetup
{
  bool operator==(const ezGALRenderingSetup& other) const;
  bool operator!=(const ezGALRenderingSetup& other) const;

  ezGALRenderTargetSetup m_RenderTargetSetup;
  ezColor m_ClearColor = ezColor(0, 0, 0, 0);
  ezUInt32 m_uiRenderTargetClearMask = 0x0;
  float m_fDepthClear = 1.0f;
  ezUInt8 m_uiStencilClear = 0;
  bool m_bClearDepth = false;
  bool m_bClearStencil = false;
  bool m_bDiscardColor = false;
  bool m_bDiscardDepth = false;
};

#include <RendererFoundation/Resources/Implementation/RenderTargetSetup_inl.h>
