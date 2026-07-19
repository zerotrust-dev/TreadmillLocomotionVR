ScriptName TreadmillLocomotionVR_MCM Extends SKI_ConfigBase

Int Property ExpectedNativeApiVersion = 1 AutoReadOnly

Int _enabledOID
Int _jumpOutputOID
Int _presetGentleOID
Int _presetBalancedOID
Int _presetAthleticOID
Int _riseThresholdOID
Int _upwardVelocityOID
Int _cooldownOID
Int _triggerOnRiseOID
Int _fallThresholdOID
Int _jumpPulseOID
Int _rightStickYOID
Int _telemetryOID
Int _debugLoggingOID

Bool _enabled
Bool _jumpOutput
Bool _triggerOnRise
Bool _telemetry
Bool _debugLogging

Float _riseThreshold
Float _fallThreshold
Float _upwardVelocity
Float _cooldown
Float _jumpPulse
Float _rightStickY

Int Function GetVersion()
	Return 3
EndFunction

Event OnConfigInit()
	ModName = "Treadmill Locomotion VR"
	Pages = New String[2]
	Pages[0] = "Quick Setup"
	Pages[1] = "Advanced"
EndEvent

Event OnConfigOpen()
	If TreadmillLocomotionVR.GetNativeApiVersion() != ExpectedNativeApiVersion
		Debug.Notification("Treadmill Locomotion VR MCM/API version mismatch")
	EndIf
	LoadSettings()
EndEvent

Event OnConfigClose()
	SaveAndApply()
EndEvent

Event OnPageReset(String page)
	LoadSettings()
	SetCursorFillMode(TOP_TO_BOTTOM)

	If page == "" || page == "Quick Setup"
		DrawQuickSetup()
	ElseIf page == "Advanced"
		DrawAdvanced()
	EndIf
EndEvent

Event OnOptionHighlight(Int option)
	If option == _enabledOID
		SetInfoText("Turns the gesture detector on or off. Leave enabled for normal play.")
	ElseIf option == _jumpOutputOID
		SetInfoText("Sends the jump input when the gesture is detected. Turn this off to log/test without jumping.")
	ElseIf option == _presetGentleOID
		SetInfoText("Easier for slower or smaller movements. Use this if you do not want to push hard with your legs.")
	ElseIf option == _presetBalancedOID
		SetInfoText("Recommended setup from testing. Good first choice for most users.")
	ElseIf option == _presetAthleticOID
		SetInfoText("Requires a quicker upward push. Use this if slow standing up causes unwanted jumps.")
	ElseIf option == _riseThresholdOID
		SetInfoText("How much your headset must rise before a jump can start. Lower is more sensitive; higher requires a bigger motion.")
	ElseIf option == _upwardVelocityOID
		SetInfoText("How fast you must move upward. Lower helps older or slower users; higher rejects slow standing up.")
	ElseIf option == _cooldownOID
		SetInfoText("Minimum time between jumps. Raise this if one movement sometimes jumps twice.")
	ElseIf option == _triggerOnRiseOID
		SetInfoText("Jumps as soon as the upward push is detected. Recommended because it feels responsive.")
	ElseIf option == _fallThresholdOID
		SetInfoText("How much you must settle back down for the full gesture to be confirmed in telemetry.")
	ElseIf option == _jumpPulseOID
		SetInfoText("How long the virtual right-stick-up jump input is held. Increase only if Skyrim misses jumps.")
	ElseIf option == _rightStickYOID
		SetInfoText("Strength of virtual right-stick-up. Leave at 1.00 unless debugging input.")
	ElseIf option == _telemetryOID
		SetInfoText("Writes a CSV in the SKSE log folder for tuning. Useful during testing, off for normal play.")
	ElseIf option == _debugLoggingOID
		SetInfoText("Writes extra debug details. Leave off unless troubleshooting.")
	EndIf
EndEvent

Event OnOptionSelect(Int option)
	If option == _enabledOID
		_enabled = !_enabled
		TreadmillLocomotionVR.SetBoolSetting("Enabled", _enabled)
		SetToggleOptionValue(option, _enabled)
		SaveAndApply()
	ElseIf option == _jumpOutputOID
		_jumpOutput = !_jumpOutput
		TreadmillLocomotionVR.SetBoolSetting("JumpOutput", _jumpOutput)
		SetToggleOptionValue(option, _jumpOutput)
		SaveAndApply()
	ElseIf option == _presetGentleOID
		ApplyPreset(0)
	ElseIf option == _presetBalancedOID
		ApplyPreset(1)
	ElseIf option == _presetAthleticOID
		ApplyPreset(2)
	ElseIf option == _triggerOnRiseOID
		_triggerOnRise = !_triggerOnRise
		TreadmillLocomotionVR.SetBoolSetting("TriggerOnRise", _triggerOnRise)
		SetToggleOptionValue(option, _triggerOnRise)
		SaveAndApply()
	ElseIf option == _telemetryOID
		_telemetry = !_telemetry
		TreadmillLocomotionVR.SetBoolSetting("Telemetry", _telemetry)
		SetToggleOptionValue(option, _telemetry)
		SaveAndApply()
	ElseIf option == _debugLoggingOID
		_debugLogging = !_debugLogging
		TreadmillLocomotionVR.SetBoolSetting("DebugLogging", _debugLogging)
		SetToggleOptionValue(option, _debugLogging)
		SaveAndApply()
	EndIf
EndEvent

Event OnOptionSliderOpen(Int option)
	If option == _riseThresholdOID
		OpenSlider(_riseThreshold, 0.040, 0.015, 0.120, 0.005)
	ElseIf option == _upwardVelocityOID
		OpenSlider(_upwardVelocity, 0.25, 0.05, 1.00, 0.05)
	ElseIf option == _cooldownOID
		OpenSlider(_cooldown, 0.70, 0.20, 2.00, 0.05)
	ElseIf option == _fallThresholdOID
		OpenSlider(_fallThreshold, 0.020, 0.010, 0.080, 0.005)
	ElseIf option == _jumpPulseOID
		OpenSlider(_jumpPulse, 0.12, 0.02, 0.50, 0.01)
	ElseIf option == _rightStickYOID
		OpenSlider(_rightStickY, 1.00, 0.25, 1.00, 0.05)
	EndIf
EndEvent

Event OnOptionSliderAccept(Int option, Float value)
	If option == _riseThresholdOID
		_riseThreshold = value
		TreadmillLocomotionVR.SetFloatSetting("RiseThresholdMeters", value)
		SetSliderOptionValue(option, value, "{3} m")
	ElseIf option == _upwardVelocityOID
		_upwardVelocity = value
		TreadmillLocomotionVR.SetFloatSetting("MinUpwardVelocityMetersPerSecond", value)
		SetSliderOptionValue(option, value, "{2} m/s")
	ElseIf option == _cooldownOID
		_cooldown = value
		TreadmillLocomotionVR.SetFloatSetting("CooldownSeconds", value)
		SetSliderOptionValue(option, value, "{2} s")
	ElseIf option == _fallThresholdOID
		_fallThreshold = value
		TreadmillLocomotionVR.SetFloatSetting("FallThresholdMeters", value)
		SetSliderOptionValue(option, value, "{3} m")
	ElseIf option == _jumpPulseOID
		_jumpPulse = value
		TreadmillLocomotionVR.SetFloatSetting("JumpPulseSeconds", value)
		SetSliderOptionValue(option, value, "{2} s")
	ElseIf option == _rightStickYOID
		_rightStickY = value
		TreadmillLocomotionVR.SetFloatSetting("RightStickY", value)
		SetSliderOptionValue(option, value, "{2}")
	EndIf

	SaveAndApply()
EndEvent

Function DrawQuickSetup()
	AddHeaderOption("Status")
	_enabledOID = AddToggleOption("Tiptoe jump detection", _enabled)
	_jumpOutputOID = AddToggleOption("Send jump input", _jumpOutput)
	AddTextOption("Native plugin", TreadmillLocomotionVR.GetPluginVersion())

	AddHeaderOption("Presets")
	_presetGentleOID = AddTextOption("Apply Gentle", "Slower")
	_presetBalancedOID = AddTextOption("Apply Balanced", "Recommended")
	_presetAthleticOID = AddTextOption("Apply Athletic", "Quicker")

	AddHeaderOption("Most useful tuning")
	_riseThresholdOID = AddSliderOption("Rise needed", _riseThreshold, "{3} m")
	_upwardVelocityOID = AddSliderOption("Upward speed", _upwardVelocity, "{2} m/s")
	_cooldownOID = AddSliderOption("Time between jumps", _cooldown, "{2} s")
EndFunction

Function DrawAdvanced()
	AddHeaderOption("Gesture")
	_triggerOnRiseOID = AddToggleOption("Jump on upward push", _triggerOnRise)
	_fallThresholdOID = AddSliderOption("Settle amount", _fallThreshold, "{3} m")

	AddHeaderOption("Input")
	_jumpPulseOID = AddSliderOption("Jump input hold time", _jumpPulse, "{2} s")
	_rightStickYOID = AddSliderOption("Right-stick-up strength", _rightStickY, "{2}")

	AddHeaderOption("Diagnostics")
	_telemetryOID = AddToggleOption("Telemetry CSV", _telemetry)
	_debugLoggingOID = AddToggleOption("Debug logging", _debugLogging)
EndFunction

Function LoadSettings()
	_enabled = TreadmillLocomotionVR.GetBoolSetting("Enabled")
	_jumpOutput = TreadmillLocomotionVR.GetBoolSetting("JumpOutput")
	_triggerOnRise = TreadmillLocomotionVR.GetBoolSetting("TriggerOnRise")
	_telemetry = TreadmillLocomotionVR.GetBoolSetting("Telemetry")
	_debugLogging = TreadmillLocomotionVR.GetBoolSetting("DebugLogging")

	_riseThreshold = TreadmillLocomotionVR.GetFloatSetting("RiseThresholdMeters")
	_fallThreshold = TreadmillLocomotionVR.GetFloatSetting("FallThresholdMeters")
	_upwardVelocity = TreadmillLocomotionVR.GetFloatSetting("MinUpwardVelocityMetersPerSecond")
	_cooldown = TreadmillLocomotionVR.GetFloatSetting("CooldownSeconds")
	_jumpPulse = TreadmillLocomotionVR.GetFloatSetting("JumpPulseSeconds")
	_rightStickY = TreadmillLocomotionVR.GetFloatSetting("RightStickY")
EndFunction

Function OpenSlider(Float currentValue, Float defaultValue, Float minValue, Float maxValue, Float interval)
	SetSliderDialogStartValue(currentValue)
	SetSliderDialogDefaultValue(defaultValue)
	SetSliderDialogRange(minValue, maxValue)
	SetSliderDialogInterval(interval)
EndFunction

Function ApplyPreset(Int preset)
	TreadmillLocomotionVR.SetBoolSetting("Enabled", True)
	TreadmillLocomotionVR.SetBoolSetting("JumpOutput", True)
	TreadmillLocomotionVR.SetBoolSetting("TriggerOnRise", True)

	If preset == 0
		TreadmillLocomotionVR.SetFloatSetting("RiseThresholdMeters", 0.030)
		TreadmillLocomotionVR.SetFloatSetting("FallThresholdMeters", 0.015)
		TreadmillLocomotionVR.SetFloatSetting("MinUpwardVelocityMetersPerSecond", 0.15)
		TreadmillLocomotionVR.SetFloatSetting("CooldownSeconds", 0.80)
	ElseIf preset == 1
		TreadmillLocomotionVR.SetFloatSetting("RiseThresholdMeters", 0.040)
		TreadmillLocomotionVR.SetFloatSetting("FallThresholdMeters", 0.020)
		TreadmillLocomotionVR.SetFloatSetting("MinUpwardVelocityMetersPerSecond", 0.25)
		TreadmillLocomotionVR.SetFloatSetting("CooldownSeconds", 0.70)
	ElseIf preset == 2
		TreadmillLocomotionVR.SetFloatSetting("RiseThresholdMeters", 0.045)
		TreadmillLocomotionVR.SetFloatSetting("FallThresholdMeters", 0.020)
		TreadmillLocomotionVR.SetFloatSetting("MinUpwardVelocityMetersPerSecond", 0.35)
		TreadmillLocomotionVR.SetFloatSetting("CooldownSeconds", 0.60)
	EndIf

	TreadmillLocomotionVR.SetFloatSetting("JumpPulseSeconds", 0.12)
	TreadmillLocomotionVR.SetFloatSetting("RightStickY", 1.00)
	SaveAndApply()
	ForcePageReset()
EndFunction

Function SaveAndApply()
	If TreadmillLocomotionVR.GetNativeApiVersion() != ExpectedNativeApiVersion
		Debug.Notification("Treadmill Locomotion VR MCM/API version mismatch")
		Return
	EndIf

	TreadmillLocomotionVR.SaveSettings()
	TreadmillLocomotionVR.ApplySettings()
EndFunction
