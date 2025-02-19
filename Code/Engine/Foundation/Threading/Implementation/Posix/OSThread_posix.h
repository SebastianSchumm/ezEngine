#include <Foundation/FoundationInternal.h>
EZ_FOUNDATION_INTERNAL_HEADER

ezAtomicInteger32 ezOSThread::s_iThreadCount;

// Posix specific implementation of the thread class

ezOSThread::ezOSThread(
  ezOSThreadEntryPoint pThreadEntryPoint, void* pUserData /*= nullptr*/, const char* szName /*= "ezThread"*/, ezUInt32 uiStackSize /*= 128 * 1024*/)
{
  s_iThreadCount.Increment();

  m_EntryPoint = pThreadEntryPoint;
  m_pUserData = pUserData;
  m_szName = szName;
  m_uiStackSize = uiStackSize;

  // Thread creation is deferred since Posix threads can't be created sleeping
}

ezOSThread::~ezOSThread()
{
  s_iThreadCount.Decrement();
}

/// Starts the thread
void ezOSThread::Start()
{
  pthread_attr_t ThreadAttributes;
  pthread_attr_init(&ThreadAttributes);
  pthread_attr_setdetachstate(&ThreadAttributes, PTHREAD_CREATE_JOINABLE);
  pthread_attr_setstacksize(&ThreadAttributes, m_uiStackSize);

  int iReturnCode = pthread_create(&m_Handle, &ThreadAttributes, m_EntryPoint, m_pUserData);
  EZ_IGNORE_UNUSED(iReturnCode);
  EZ_ASSERT_RELEASE(iReturnCode == 0, "Thread creation failed!");

#if EZ_ENABLED(EZ_PLATFORM_LINUX) || EZ_ENABLED(EZ_PLATFORM_ANDROID)
  if (iReturnCode == 0 && m_szName != nullptr)
  {
    // pthread has a thread name limit of 16 bytes.
    // This means 15 characters and the terminating '\0'
    if (strlen(m_szName) < 16)
    {
      pthread_setname_np(m_Handle, m_szName);
    }
    else
    {
      char threadName[16];
      strncpy(threadName, m_szName, 15);
      threadName[15] = '\0';
      pthread_setname_np(m_Handle, threadName);
    }
  }
#endif

  m_ThreadID = m_Handle;

  pthread_attr_destroy(&ThreadAttributes);
}

/// Joins with the thread (waits for termination)
void ezOSThread::Join()
{
  pthread_join(m_Handle, nullptr);
}
