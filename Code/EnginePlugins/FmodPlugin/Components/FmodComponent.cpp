#include <FmodPlugin/FmodPluginPCH.h>

#include <FmodPlugin/Components/FmodComponent.h>
#include <FmodPlugin/FmodIncludes.h>

// clang-format off
EZ_BEGIN_ABSTRACT_COMPONENT_TYPE(ezFmodComponent, 1)
{
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Sound/FMOD"),
  }
  EZ_END_ATTRIBUTES;
}
EZ_END_ABSTRACT_COMPONENT_TYPE;
// clang-format on

ezFmodComponent::ezFmodComponent() = default;
ezFmodComponent::~ezFmodComponent() = default;


EZ_STATICLINK_FILE(FmodPlugin, FmodPlugin_Components_FmodComponent);
