#include <Foundation/FoundationPCH.h>

#include <Foundation/Logging/Log.h>
#include <Foundation/Strings/HashedString.h>
#include <Foundation/Threading/Lock.h>
#include <Foundation/Threading/Mutex.h>

struct HashedStringData
{
  ezMutex m_Mutex;
  ezHashedString::StringStorage m_Storage;
  ezHashedString::HashedType m_Empty;
};

static HashedStringData* s_pHSData;

EZ_MSVC_ANALYSIS_WARNING_PUSH
EZ_MSVC_ANALYSIS_WARNING_DISABLE(6011) // Disable warning for null pointer dereference as InitHashedString() will ensure that s_pHSData is set

// static
ezHashedString::HashedType ezHashedString::AddHashedString(ezStringView szString, ezUInt64 uiHash)
{
  if (s_pHSData == nullptr)
    InitHashedString();

  EZ_LOCK(s_pHSData->m_Mutex);

  // try to find the existing string
  bool bExisted = false;
  auto ret = s_pHSData->m_Storage.FindOrAdd(uiHash, &bExisted);

  // if it already exists, just increase the refcount
  if (bExisted)
  {
#if EZ_ENABLED(EZ_COMPILE_FOR_DEVELOPMENT)
    if (ret.Value().m_sString != szString)
    {
      // TODO: I think this should be a more serious issue
      ezLog::Error("Hash collision encountered: Strings \"{}\" and \"{}\" both hash to {}.", ezArgSensitive(ret.Value().m_sString), ezArgSensitive(szString), uiHash);
    }
#endif

#if EZ_ENABLED(EZ_HASHED_STRING_REF_COUNTING)
    ret.Value().m_iRefCount.Increment();
#endif
  }
  else
  {
    ezHashedString::HashedData& d = ret.Value();
#if EZ_ENABLED(EZ_HASHED_STRING_REF_COUNTING)
    d.m_iRefCount = 1;
#endif
    d.m_sString = szString;
  }

  return ret;
}

EZ_MSVC_ANALYSIS_WARNING_POP

// static
void ezHashedString::InitHashedString()
{
  if (s_pHSData != nullptr)
    return;

  alignas(EZ_ALIGNMENT_OF(HashedStringData)) static ezUInt8 HashedStringDataBuffer[sizeof(HashedStringData)];
  s_pHSData = new (HashedStringDataBuffer) HashedStringData();

  // makes sure the empty string exists for the default constructor to use
  s_pHSData->m_Empty = AddHashedString("", ezHashingUtils::StringHash(""));

#if EZ_ENABLED(EZ_HASHED_STRING_REF_COUNTING)
  // this one should never get deleted, so make sure its refcount is 2
  s_pHSData->m_Empty.Value().m_iRefCount.Increment();
#endif
}

#if EZ_ENABLED(EZ_HASHED_STRING_REF_COUNTING)
ezUInt32 ezHashedString::ClearUnusedStrings()
{
  EZ_LOCK(s_pHSData->m_Mutex);

  ezUInt32 uiDeleted = 0;

  for (auto it = s_pHSData->m_Storage.GetIterator(); it.IsValid();)
  {
    if (it.Value().m_iRefCount == 0)
    {
      it = s_pHSData->m_Storage.Remove(it);
      ++uiDeleted;
    }
    else
      ++it;
  }

  return uiDeleted;
}
#endif

EZ_MSVC_ANALYSIS_WARNING_PUSH
EZ_MSVC_ANALYSIS_WARNING_DISABLE(6011) // Disable warning for null pointer dereference as InitHashedString() will ensure that s_pHSData is set

ezHashedString::ezHashedString()
{
  EZ_CHECK_AT_COMPILETIME_MSG(sizeof(m_Data) == sizeof(void*), "The hashed string data should only be as large as one pointer.");
  EZ_CHECK_AT_COMPILETIME_MSG(sizeof(*this) == sizeof(void*), "The hashed string data should only be as large as one pointer.");

  // only insert the empty string once, after that, we can just use it without the need for the mutex
  if (s_pHSData == nullptr)
    InitHashedString();

  m_Data = s_pHSData->m_Empty;
#if EZ_ENABLED(EZ_HASHED_STRING_REF_COUNTING)
  m_Data.Value().m_iRefCount.Increment();
#endif
}

EZ_MSVC_ANALYSIS_WARNING_POP

bool ezHashedString::IsEmpty() const
{
  return m_Data == s_pHSData->m_Empty;
}

void ezHashedString::Clear()
{
#if EZ_ENABLED(EZ_HASHED_STRING_REF_COUNTING)
  if (m_Data != s_pHSData->m_Empty)
  {
    HashedType tmp = m_Data;

    m_Data = s_pHSData->m_Empty;
    m_Data.Value().m_iRefCount.Increment();

    tmp.Value().m_iRefCount.Decrement();
  }
#else
  m_Data = s_pHSData->m_Empty;
#endif
}

EZ_STATICLINK_FILE(Foundation, Foundation_Strings_Implementation_HashedString);
