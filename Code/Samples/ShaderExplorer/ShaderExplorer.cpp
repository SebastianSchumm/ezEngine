#include <ShaderExplorer/ShaderExplorer.h>

#include <Core/Graphics/Camera.h>
#include <Core/Graphics/Geometry.h>
#include <Core/Input/InputManager.h>
#include <Core/ResourceManager/ResourceManager.h>
#include <Core/System/Window.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/FileSystem/DataDirTypeFolder.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/Logging/ConsoleWriter.h>
#include <Foundation/Logging/Log.h>
#include <Foundation/Logging/VisualStudioWriter.h>
#include <Foundation/Time/Clock.h>
#include <RendererCore/Meshes/MeshBufferResource.h>
#include <RendererCore/RenderContext/RenderContext.h>
#include <RendererCore/ShaderCompiler/ShaderManager.h>
#include <RendererCore/Textures/Texture2DResource.h>
#include <RendererDX11/Device/DeviceDX11.h>
#include <RendererFoundation/Descriptors/Descriptors.h>
#include <RendererFoundation/Device/Device.h>
#include <RendererFoundation/Device/DeviceFactory.h>
#include <RendererFoundation/Device/SwapChain.h>
#include <RendererFoundation/Resources/RenderTargetSetup.h>

static ezUInt32 g_uiWindowWidth = 640;
static ezUInt32 g_uiWindowHeight = 480;

class ezShaderExplorerWindow : public ezWindow
{
public:
  ezShaderExplorerWindow()
    : ezWindow()
  {
    m_bCloseRequested = false;
  }

  virtual void OnClickClose() override { m_bCloseRequested = true; }

  bool m_bCloseRequested;
};

ezShaderExplorerApp::ezShaderExplorerApp()
  : ezApplication("Shader Explorer")
{
}

ezApplication::Execution ezShaderExplorerApp::Run()
{
  m_pWindow->ProcessWindowMessages();

  if (m_pWindow->m_bCloseRequested || ezInputManager::GetInputActionState("Main", "CloseApp") == ezKeyState::Pressed)
    return Execution::Quit;

  // make sure time goes on
  ezClock::GetGlobalClock()->Update();

  // update all input state
  ezInputManager::Update(ezClock::GetGlobalClock()->GetTimeDiff());

  // mouse look
  if (ezInputManager::GetInputActionState("Main", "Look") == ezKeyState::Down)
  {
    m_pWindow->GetInputDevice()->SetShowMouseCursor(false);
    m_pWindow->GetInputDevice()->SetClipMouseCursor(ezMouseCursorClipMode::ClipToPosition);

    float fInputValue = 0.0f;
    const float fMouseSpeed = 0.01f;

    ezVec3 mouseMotion(0.0f);

    if (ezInputManager::GetInputActionState("Main", "LookPosX", &fInputValue) != ezKeyState::Up)
      mouseMotion.x += fInputValue * fMouseSpeed;
    if (ezInputManager::GetInputActionState("Main", "LookNegX", &fInputValue) != ezKeyState::Up)
      mouseMotion.x -= fInputValue * fMouseSpeed;
    if (ezInputManager::GetInputActionState("Main", "LookPosY", &fInputValue) != ezKeyState::Up)
      mouseMotion.y -= fInputValue * fMouseSpeed;
    if (ezInputManager::GetInputActionState("Main", "LookNegY", &fInputValue) != ezKeyState::Up)
      mouseMotion.y += fInputValue * fMouseSpeed;

    m_camera->RotateLocally(ezAngle::Radian(0.0), ezAngle::Radian(mouseMotion.y), ezAngle::Radian(0.0));
    m_camera->RotateGlobally(ezAngle::Radian(0.0), ezAngle::Radian(mouseMotion.x), ezAngle::Radian(0.0));
  }
  else
  {
    m_pWindow->GetInputDevice()->SetShowMouseCursor(true);
    m_pWindow->GetInputDevice()->SetClipMouseCursor(ezMouseCursorClipMode::NoClip);
  }

  // turn camera with keys
  {
    float fInputValue = 0.0f;
    const float fTurnSpeed = 1.0f;

    ezVec3 mouseMotion(0.0f);

    if (ezInputManager::GetInputActionState("Main", "TurnPosX", &fInputValue) != ezKeyState::Up)
      mouseMotion.x += fInputValue * fTurnSpeed;
    if (ezInputManager::GetInputActionState("Main", "TurnNegX", &fInputValue) != ezKeyState::Up)
      mouseMotion.x -= fInputValue * fTurnSpeed;
    if (ezInputManager::GetInputActionState("Main", "TurnPosY", &fInputValue) != ezKeyState::Up)
      mouseMotion.y += fInputValue * fTurnSpeed;
    if (ezInputManager::GetInputActionState("Main", "TurnNegY", &fInputValue) != ezKeyState::Up)
      mouseMotion.y -= fInputValue * fTurnSpeed;

    m_camera->RotateLocally(ezAngle::Radian(0.0), ezAngle::Radian(mouseMotion.y), ezAngle::Radian(0.0));
    m_camera->RotateGlobally(ezAngle::Radian(0.0), ezAngle::Radian(mouseMotion.x), ezAngle::Radian(0.0));
  }

  // movement
  {
    float fInputValue = 0.0f;
    ezVec3 cameraMotion(0.0f);

    if (ezInputManager::GetInputActionState("Main", "MovePosX", &fInputValue) != ezKeyState::Up)
      cameraMotion.x += fInputValue;
    if (ezInputManager::GetInputActionState("Main", "MoveNegX", &fInputValue) != ezKeyState::Up)
      cameraMotion.x -= fInputValue;
    if (ezInputManager::GetInputActionState("Main", "MovePosY", &fInputValue) != ezKeyState::Up)
      cameraMotion.y += fInputValue;
    if (ezInputManager::GetInputActionState("Main", "MoveNegY", &fInputValue) != ezKeyState::Up)
      cameraMotion.y -= fInputValue;

    m_camera->MoveLocally(cameraMotion.y, cameraMotion.x, 0.0f);
  }


  m_stuffChanged = false;
  m_directoryWatcher->EnumerateChanges(ezMakeDelegate(&ezShaderExplorerApp::OnFileChanged, this));
  if (m_stuffChanged)
  {
    ezResourceManager::ReloadAllResources(false);
  }

  // do the rendering
  {
    // Before starting to render in a frame call this function
    m_pDevice->BeginFrame();

    m_pDevice->BeginPipeline("ShaderExplorer", m_hSwapChain);

    ezGALPass* pGALPass = m_pDevice->BeginPass("ezShaderExplorerMainPass");
    const ezGALSwapChain* pPrimarySwapChain = m_pDevice->GetSwapChain(m_hSwapChain);
    ezGALRenderTargetViewHandle hBBRTV = m_pDevice->GetDefaultRenderTargetView(pPrimarySwapChain->GetRenderTargets().m_hRTs[0]);
    ezGALRenderTargetViewHandle hBBDSV = m_pDevice->GetDefaultRenderTargetView(m_hDepthStencilTexture);

    ezGALRenderingSetup renderingSetup;
    renderingSetup.m_RenderTargetSetup.SetRenderTarget(0, hBBRTV).SetDepthStencilTarget(hBBDSV);
    renderingSetup.m_uiRenderTargetClearMask = 0xFFFFFFFF;
    renderingSetup.m_bClearDepth = true;
    renderingSetup.m_bClearStencil = true;

    ezGALRenderCommandEncoder* pCommandEncoder = ezRenderContext::GetDefaultInstance()->BeginRendering(pGALPass, renderingSetup, ezRectFloat(0.0f, 0.0f, (float)g_uiWindowWidth, (float)g_uiWindowHeight));

    auto& gc = ezRenderContext::GetDefaultInstance()->WriteGlobalConstants();
    ezMemoryUtils::ZeroFill(&gc, 1);

    gc.WorldToCameraMatrix[0] = m_camera->GetViewMatrix(ezCameraEye::Left);
    gc.WorldToCameraMatrix[1] = m_camera->GetViewMatrix(ezCameraEye::Right);
    gc.CameraToWorldMatrix[0] = gc.WorldToCameraMatrix[0].GetInverse();
    gc.CameraToWorldMatrix[1] = gc.WorldToCameraMatrix[1].GetInverse();
    gc.ViewportSize = ezVec4((float)g_uiWindowWidth, (float)g_uiWindowHeight, 1.0f / (float)g_uiWindowWidth, 1.0f / (float)g_uiWindowHeight);
    // Wrap around to prevent floating point issues. Wrap around is dividable by all whole numbers up to 11.
    gc.GlobalTime = (float)ezMath::Mod(ezClock::GetGlobalClock()->GetAccumulatedTime().GetSeconds(), 20790.0);
    gc.WorldTime = gc.GlobalTime;

    ezRenderContext::GetDefaultInstance()->BindMaterial(m_hMaterial);
    ezRenderContext::GetDefaultInstance()->BindMeshBuffer(m_hQuadMeshBuffer);
    ezRenderContext::GetDefaultInstance()->DrawMeshBuffer().IgnoreResult();

    ezRenderContext::GetDefaultInstance()->EndRendering();
    m_pDevice->EndPass(pGALPass);

    m_pDevice->EndPipeline(m_hSwapChain);

    m_pDevice->EndFrame();
    ezRenderContext::GetDefaultInstance()->ResetContextState();
  }

  // needs to be called once per frame
  ezResourceManager::PerFrameUpdate();

  // tell the task system to finish its work for this frame
  // this has to be done at the very end, so that the task system will only use up the time that is left in this frame for
  // uploading GPU data etc.
  ezTaskSystem::FinishFrameTasks();

  return ezApplication::Execution::Continue;
}

void ezShaderExplorerApp::AfterCoreSystemsStartup()
{
  m_camera = EZ_DEFAULT_NEW(ezCamera);
  m_camera->LookAt(ezVec3(3, 3, 1.5), ezVec3(0, 0, 0), ezVec3(0, 1, 0));
  m_directoryWatcher = EZ_DEFAULT_NEW(ezDirectoryWatcher);

  ezStringBuilder sProjectDir = ">sdk/Data/Samples/ShaderExplorer";
  ezStringBuilder sProjectDirResolved;
  ezFileSystem::ResolveSpecialDirectory(sProjectDir, sProjectDirResolved).IgnoreResult();

  ezFileSystem::SetSpecialDirectory("project", sProjectDirResolved);

  EZ_VERIFY(m_directoryWatcher->OpenDirectory(sProjectDirResolved, ezDirectoryWatcher::Watch::Writes | ezDirectoryWatcher::Watch::Subdirectories).Succeeded(), "Failed to watch project directory");

  ezFileSystem::AddDataDirectory("", "", ":", ezFileSystem::AllowWrites).IgnoreResult();
  ezFileSystem::AddDataDirectory(">appdir/", "AppBin", "bin", ezFileSystem::AllowWrites).IgnoreResult();                                   // writing to the binary directory
  ezFileSystem::AddDataDirectory(">appdir/", "ShaderCache", "shadercache", ezFileSystem::AllowWrites).IgnoreResult();                      // for shader files
  ezFileSystem::AddDataDirectory(">user/ezEngine Project/ShaderExplorer", "AppData", "appdata", ezFileSystem::AllowWrites).IgnoreResult(); // app user data

  ezFileSystem::AddDataDirectory(">sdk/Data/Base", "Base", "base").IgnoreResult();
  ezFileSystem::AddDataDirectory(">project/", "Project", "project", ezFileSystem::AllowWrites).IgnoreResult();

  ezGlobalLog::AddLogWriter(ezLogWriter::Console::LogMessageHandler);
  ezGlobalLog::AddLogWriter(ezLogWriter::VisualStudio::LogMessageHandler);

  ezPlugin::LoadPlugin("ezInspectorPlugin").IgnoreResult();

#ifdef BUILDSYSTEM_ENABLE_VULKAN_SUPPORT
  constexpr const char* szDefaultRenderer = "Vulkan";
#else
  constexpr const char* szDefaultRenderer = "DX11";
#endif

  const char* szRendererName = ezCommandLineUtils::GetGlobalInstance()->GetStringOption("-renderer", 0, szDefaultRenderer);
  const char* szShaderModel = "";
  const char* szShaderCompiler = "";
  ezGALDeviceFactory::GetShaderModelAndCompiler(szRendererName, szShaderModel, szShaderCompiler);

  ezShaderManager::Configure(szShaderModel, true);
  EZ_VERIFY(ezPlugin::LoadPlugin(szShaderCompiler).Succeeded(), "Shader compiler '{}' plugin not found", szShaderCompiler);

  // Register Input
  {
    ezInputActionConfig cfg;

    cfg = ezInputManager::GetInputActionConfig("Main", "CloseApp");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyEscape;
    ezInputManager::SetInputActionConfig("Main", "CloseApp", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "LookPosX");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_MouseMovePosX;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "LookPosX", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "LookNegX");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_MouseMoveNegX;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "LookNegX", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "LookPosY");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_MouseMovePosY;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "LookPosY", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "LookNegY");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_MouseMoveNegY;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "LookNegY", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "TurnPosX");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyRight;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "TurnPosX", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "TurnNegX");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyLeft;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "TurnNegX", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "TurnPosY");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyDown;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "TurnPosY", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "TurnNegY");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyUp;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "TurnNegY", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "Look");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_MouseButton0;
    cfg.m_bApplyTimeScaling = false;
    ezInputManager::SetInputActionConfig("Main", "Look", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "MovePosX");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyD;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "MovePosX", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "MoveNegX");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyA;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "MoveNegX", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "MovePosY");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyW;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "MovePosY", cfg, true);

    cfg = ezInputManager::GetInputActionConfig("Main", "MoveNegY");
    cfg.m_sInputSlotTrigger[0] = ezInputSlot_KeyS;
    cfg.m_bApplyTimeScaling = true;
    ezInputManager::SetInputActionConfig("Main", "MoveNegY", cfg, true);
  }

  // Create a window for rendering
  {
    ezWindowCreationDesc WindowCreationDesc;
    WindowCreationDesc.m_Resolution.width = g_uiWindowWidth;
    WindowCreationDesc.m_Resolution.height = g_uiWindowHeight;
    WindowCreationDesc.m_Title = "Shader Explorer";
    WindowCreationDesc.m_bShowMouseCursor = true;
    WindowCreationDesc.m_bClipMouseCursor = false;
    WindowCreationDesc.m_WindowMode = ezWindowMode::WindowResizable;
    m_pWindow = EZ_DEFAULT_NEW(ezShaderExplorerWindow);
    m_pWindow->Initialize(WindowCreationDesc).IgnoreResult();
  }

  // Create a device
  {
    ezGALDeviceCreationDescription DeviceInit;
    DeviceInit.m_bDebugDevice = true;

    m_pDevice = ezGALDeviceFactory::CreateDevice(szRendererName, ezFoundation::GetDefaultAllocator(), DeviceInit);
    EZ_ASSERT_DEV(m_pDevice != nullptr, "Device implemention for '{}' not found", szRendererName);
    EZ_VERIFY(m_pDevice->Init() == EZ_SUCCESS, "Device init failed!");

    ezGALDevice::SetDefaultDevice(m_pDevice);
  }

  // now that we have a window and device, tell the engine to initialize the rendering infrastructure
  ezStartup::StartupHighLevelSystems();

  // Create a Swapchain
  {
    ezGALWindowSwapChainCreationDescription swapChainDesc;
    swapChainDesc.m_pWindow = m_pWindow;
    swapChainDesc.m_SampleCount = ezGALMSAASampleCount::None;
    swapChainDesc.m_bAllowScreenshots = true;
    swapChainDesc.m_PresentMode = ezGALPresentMode::VSync;
    m_hSwapChain = ezGALWindowSwapChain::Create(swapChainDesc);

    ezGALTextureCreationDescription texDesc;
    texDesc.m_uiWidth = g_uiWindowWidth;
    texDesc.m_uiHeight = g_uiWindowHeight;
    texDesc.m_Format = ezGALResourceFormat::D24S8;
    texDesc.m_bCreateRenderTarget = true;

    m_hDepthStencilTexture = m_pDevice->CreateTexture(texDesc);
  }

  // Setup Shaders and Materials
  {
    m_hMaterial = ezResourceManager::LoadResource<ezMaterialResource>("Materials/screen.ezMaterial");

    // Create the mesh that we use for rendering
    CreateScreenQuad();
  }
}

void ezShaderExplorerApp::BeforeHighLevelSystemsShutdown()
{
  m_directoryWatcher->CloseDirectory();

  m_pDevice->DestroyTexture(m_hDepthStencilTexture);
  m_hDepthStencilTexture.Invalidate();

  m_hMaterial.Invalidate();
  m_hQuadMeshBuffer.Invalidate();
  m_pDevice->DestroySwapChain(m_hSwapChain);

  // tell the engine that we are about to destroy window and graphics device,
  // and that it therefore needs to cleanup anything that depends on that
  ezStartup::ShutdownHighLevelSystems();

  // now we can destroy the graphics device
  m_pDevice->Shutdown().IgnoreResult();

  EZ_DEFAULT_DELETE(m_pDevice);

  // finally destroy the window
  m_pWindow->Destroy().IgnoreResult();
  EZ_DEFAULT_DELETE(m_pWindow);

  m_camera.Clear();
  m_directoryWatcher.Clear();
}

void ezShaderExplorerApp::CreateScreenQuad()
{
  ezGeometry geom;
  ezGeometry::GeoOptions opt;
  opt.m_Color = ezColor::Black;
  geom.AddRectXY(ezVec2(2, 2), 1, 1, opt);

  ezMeshBufferResourceDescriptor desc;
  desc.AddStream(ezGALVertexAttributeSemantic::Position, ezGALResourceFormat::XYZFloat);

  desc.AllocateStreams(geom.GetVertices().GetCount(), ezGALPrimitiveTopology::Triangles, geom.GetPolygons().GetCount() * 2);

  for (ezUInt32 v = 0; v < geom.GetVertices().GetCount(); ++v)
  {
    desc.SetVertexData<ezVec3>(0, v, geom.GetVertices()[v].m_vPosition);
  }

  ezUInt32 t = 0;
  for (ezUInt32 p = 0; p < geom.GetPolygons().GetCount(); ++p)
  {
    for (ezUInt32 v = 0; v < geom.GetPolygons()[p].m_Vertices.GetCount() - 2; ++v)
    {
      desc.SetTriangleIndices(t, geom.GetPolygons()[p].m_Vertices[0], geom.GetPolygons()[p].m_Vertices[v + 1], geom.GetPolygons()[p].m_Vertices[v + 2]);

      ++t;
    }
  }

  m_hQuadMeshBuffer = ezResourceManager::GetExistingResource<ezMeshBufferResource>("{E692442B-9E15-46C5-8A00-1B07C02BF8F7}");

  if (!m_hQuadMeshBuffer.IsValid())
    m_hQuadMeshBuffer = ezResourceManager::GetOrCreateResource<ezMeshBufferResource>("{E692442B-9E15-46C5-8A00-1B07C02BF8F7}", std::move(desc));
}

void ezShaderExplorerApp::OnFileChanged(const char* filename, ezDirectoryWatcherAction action, ezDirectoryWatcherType type)
{
  if (action == ezDirectoryWatcherAction::Modified && type == ezDirectoryWatcherType::File)
  {
    ezLog::Info("The file {0} was modified", filename);
    m_stuffChanged = true;
  }
}

EZ_CONSOLEAPP_ENTRY_POINT(ezShaderExplorerApp);
