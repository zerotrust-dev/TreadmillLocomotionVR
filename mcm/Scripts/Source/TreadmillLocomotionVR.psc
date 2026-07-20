ScriptName TreadmillLocomotionVR Native Hidden

Int Function GetNativeApiVersion() Global Native
String Function GetPluginVersion() Global Native

Bool Function GetBoolSetting(String name) Global Native
Float Function GetFloatSetting(String name) Global Native

Bool Function SetBoolSetting(String name, Bool value) Global Native
Bool Function SetFloatSetting(String name, Float value) Global Native

Bool Function SaveSettings() Global Native
Bool Function ReloadSettings() Global Native
Bool Function ApplySettings() Global Native
Bool Function LogMcmEvent(String message) Global Native
