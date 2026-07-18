ScriptName TreadmillLocomotionVR_MCM Extends SKI_ConfigBase

Int Property ExpectedNativeApiVersion = 1 AutoReadOnly

Int _enabledOID
Int _outputOID
Int _presetGentleOID
Int _presetBalancedOID
Int _presetFastOID
Int _deadzoneOID
Int _sprintThresholdOID
Int _sprintEnterOID
Int _sprintExitOID
Int _sprintCancelOID
Int _coastOID
Int _forwardMagnitudeOID
Int _directApiOID
Int _telemetryOID
Int _debugLoggingOID

Bool _enabled
Bool _output
Bool _directApi
Bool _telemetry
Bool _debugLogging

Float _deadzone
Float _sprintThreshold
Float _sprintEnter
Float _sprintExit
Float _sprintCancel
Float _coast
Float _forwardMagnitude

Int Function GetVersion()
	Return 1
EndFunction

Event OnConfigInit()
	ModName = "Treadmill Locomotion VR"
	Pages = New String[2]
	Pages[0] = "Tuning"
	Pages[1] = "Diagnostics"
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

	If page == "" || page == "Tuning"
		DrawTuning()
	ElseIf page == "Diagnostics"
		DrawDiagnostics()
	EndIf
EndEvent

Event OnOptionHighlight(Int option)
	If option == _enabledOID
		SetInfoText("Turns treadmill locomotion on or off. Leave on for normal play.")
	ElseIf option == _outputOID
		SetInfoText("Sends left-stick forward and sprint input to Skyrim. Turn off to log without moving.")
	ElseIf option == _presetGentleOID
		SetInfoText("Easier sprint trigger. Useful while learning the treadmill or walking carefully.")
	ElseIf option == _presetBalancedOID
		SetInfoText("Recommended starting point for testing.")
	ElseIf option == _presetFastOID
		SetInfoText("Requires a stronger treadmill signal before sprinting.")
	ElseIf option == _deadzoneOID
		SetInfoText("Minimum treadmill signal that counts as movement. Raise if tiny accidental movement starts walking.")
	ElseIf option == _sprintThresholdOID
		SetInfoText("Treadmill signal needed to sprint. This is owned by the mod, not the RealityRunner desktop curve.")
	ElseIf option == _sprintEnterOID
		SetInfoText("How long the sprint signal must be present before sprint starts. Lower reacts faster.")
	ElseIf option == _sprintExitOID
		SetInfoText("How long sprint can be absent before returning to walking. Raise if sprint flickers.")
	ElseIf option == _sprintCancelOID
		SetInfoText("Briefly releases forward after sprint ends so Skyrim stops sprinting before normal walking resumes.")
	ElseIf option == _coastOID
		SetInfoText("How long walking continues over brief missing samples. Raise slightly if movement stutters.")
	ElseIf option == _forwardMagnitudeOID
		SetInfoText("Strength of virtual left-stick-forward. Leave at 1.00 unless debugging.")
	ElseIf option == _directApiOID
		SetInfoText("Reads RealityRunner directly from COM4. Close the RealityRunner desktop app before enabling this.")
	ElseIf option == _telemetryOID
		SetInfoText("Writes intent CSV in the SKSE log folder. Keep on while tuning.")
	ElseIf option == _debugLoggingOID
		SetInfoText("Writes extra debug log details. Leave off unless troubleshooting.")
	EndIf
EndEvent

Event OnOptionSelect(Int option)
	If option == _enabledOID
		_enabled = !_enabled
		TreadmillLocomotionVR.SetBoolSetting("Enabled", _enabled)
		SetToggleOptionValue(option, _enabled)
		SaveAndApply()
	ElseIf option == _outputOID
		_output = !_output
		TreadmillLocomotionVR.SetBoolSetting("EnableOutput", _output)
		SetToggleOptionValue(option, _output)
		SaveAndApply()
	ElseIf option == _presetGentleOID
		ApplyPreset(0)
	ElseIf option == _presetBalancedOID
		ApplyPreset(1)
	ElseIf option == _presetFastOID
		ApplyPreset(2)
	ElseIf option == _telemetryOID
		_telemetry = !_telemetry
		TreadmillLocomotionVR.SetBoolSetting("Telemetry", _telemetry)
		SetToggleOptionValue(option, _telemetry)
		SaveAndApply()
	ElseIf option == _directApiOID
		_directApi = !_directApi
		TreadmillLocomotionVR.SetBoolSetting("DirectApiEnabled", _directApi)
		SetToggleOptionValue(option, _directApi)
		SaveAndApply()
	ElseIf option == _debugLoggingOID
		_debugLogging = !_debugLogging
		TreadmillLocomotionVR.SetBoolSetting("DebugLogging", _debugLogging)
		SetToggleOptionValue(option, _debugLogging)
		SaveAndApply()
	EndIf
EndEvent

Event OnOptionSliderOpen(Int option)
	If option == _deadzoneOID
		OpenSlider(_deadzone, 0.08, 0.02, 0.30, 0.01)
	ElseIf option == _sprintThresholdOID
		OpenSlider(_sprintThreshold, 0.75, 0.30, 1.00, 0.01)
	ElseIf option == _sprintEnterOID
		OpenSlider(_sprintEnter, 0.22, 0.00, 0.80, 0.01)
	ElseIf option == _sprintExitOID
		OpenSlider(_sprintExit, 0.35, 0.00, 1.00, 0.01)
	ElseIf option == _sprintCancelOID
		OpenSlider(_sprintCancel, 0.12, 0.00, 0.50, 0.01)
	ElseIf option == _coastOID
		OpenSlider(_coast, 0.25, 0.00, 1.00, 0.01)
	ElseIf option == _forwardMagnitudeOID
		OpenSlider(_forwardMagnitude, 1.00, 0.25, 1.00, 0.05)
	EndIf
EndEvent

Event OnOptionSliderAccept(Int option, Float value)
	If option == _deadzoneOID
		_deadzone = value
		TreadmillLocomotionVR.SetFloatSetting("Deadzone", value)
		SetSliderOptionValue(option, value, "{2}")
	ElseIf option == _sprintThresholdOID
		_sprintThreshold = value
		TreadmillLocomotionVR.SetFloatSetting("SprintThreshold", value)
		SetSliderOptionValue(option, value, "{2}")
	ElseIf option == _sprintEnterOID
		_sprintEnter = value
		TreadmillLocomotionVR.SetFloatSetting("SprintEnterSeconds", value)
		SetSliderOptionValue(option, value, "{2} s")
	ElseIf option == _sprintExitOID
		_sprintExit = value
		TreadmillLocomotionVR.SetFloatSetting("SprintExitSeconds", value)
		SetSliderOptionValue(option, value, "{2} s")
	ElseIf option == _sprintCancelOID
		_sprintCancel = value
		TreadmillLocomotionVR.SetFloatSetting("SprintCancelSeconds", value)
		SetSliderOptionValue(option, value, "{2} s")
	ElseIf option == _coastOID
		_coast = value
		TreadmillLocomotionVR.SetFloatSetting("CoastMaxSeconds", value)
		SetSliderOptionValue(option, value, "{2} s")
	ElseIf option == _forwardMagnitudeOID
		_forwardMagnitude = value
		TreadmillLocomotionVR.SetFloatSetting("ForwardMagnitude", value)
		SetSliderOptionValue(option, value, "{2}")
	EndIf

	SaveAndApply()
EndEvent

Function DrawTuning()
	AddHeaderOption("Status")
	_enabledOID = AddToggleOption("Treadmill locomotion", _enabled)
	_outputOID = AddToggleOption("Send movement input", _output)
	AddTextOption("Native plugin", TreadmillLocomotionVR.GetPluginVersion())

	AddHeaderOption("Presets")
	_presetGentleOID = AddTextOption("Apply Gentle", "Easy sprint")
	_presetBalancedOID = AddTextOption("Apply Balanced", "Recommended")
	_presetFastOID = AddTextOption("Apply Fast", "Harder sprint")

	AddHeaderOption("Most useful tuning")
	_deadzoneOID = AddSliderOption("Movement deadzone", _deadzone, "{2}")
	_sprintThresholdOID = AddSliderOption("Sprint threshold", _sprintThreshold, "{2}")
	_sprintEnterOID = AddSliderOption("Sprint start delay", _sprintEnter, "{2} s")
	_sprintExitOID = AddSliderOption("Sprint release delay", _sprintExit, "{2} s")
	_sprintCancelOID = AddSliderOption("Sprint cancel pulse", _sprintCancel, "{2} s")
EndFunction

Function DrawDiagnostics()
	AddHeaderOption("Smoothing")
	_coastOID = AddSliderOption("Movement coast", _coast, "{2} s")
	_forwardMagnitudeOID = AddSliderOption("Stick strength", _forwardMagnitude, "{2}")

	AddHeaderOption("Logs")
	_directApiOID = AddToggleOption("Direct RealityRunner API", _directApi)
	_telemetryOID = AddToggleOption("Telemetry CSV", _telemetry)
	_debugLoggingOID = AddToggleOption("Debug logging", _debugLogging)
EndFunction

Function LoadSettings()
	_enabled = TreadmillLocomotionVR.GetBoolSetting("Enabled")
	_output = TreadmillLocomotionVR.GetBoolSetting("EnableOutput")
	_directApi = TreadmillLocomotionVR.GetBoolSetting("DirectApiEnabled")
	_telemetry = TreadmillLocomotionVR.GetBoolSetting("Telemetry")
	_debugLogging = TreadmillLocomotionVR.GetBoolSetting("DebugLogging")

	_deadzone = TreadmillLocomotionVR.GetFloatSetting("Deadzone")
	_sprintThreshold = TreadmillLocomotionVR.GetFloatSetting("SprintThreshold")
	_sprintEnter = TreadmillLocomotionVR.GetFloatSetting("SprintEnterSeconds")
	_sprintExit = TreadmillLocomotionVR.GetFloatSetting("SprintExitSeconds")
	_sprintCancel = TreadmillLocomotionVR.GetFloatSetting("SprintCancelSeconds")
	_coast = TreadmillLocomotionVR.GetFloatSetting("CoastMaxSeconds")
	_forwardMagnitude = TreadmillLocomotionVR.GetFloatSetting("ForwardMagnitude")
EndFunction

Function OpenSlider(Float currentValue, Float defaultValue, Float minValue, Float maxValue, Float interval)
	SetSliderDialogStartValue(currentValue)
	SetSliderDialogDefaultValue(defaultValue)
	SetSliderDialogRange(minValue, maxValue)
	SetSliderDialogInterval(interval)
EndFunction

Function ApplyPreset(Int preset)
	TreadmillLocomotionVR.SetBoolSetting("Enabled", True)
	TreadmillLocomotionVR.SetBoolSetting("DirectApiEnabled", True)
	TreadmillLocomotionVR.SetBoolSetting("EnableOutput", True)
	TreadmillLocomotionVR.SetFloatSetting("Deadzone", 0.08)
	TreadmillLocomotionVR.SetFloatSetting("ForwardMagnitude", 1.00)

	If preset == 0
		TreadmillLocomotionVR.SetFloatSetting("SprintThreshold", 0.60)
		TreadmillLocomotionVR.SetFloatSetting("SprintEnterSeconds", 0.12)
		TreadmillLocomotionVR.SetFloatSetting("SprintExitSeconds", 0.45)
		TreadmillLocomotionVR.SetFloatSetting("SprintCancelSeconds", 0.12)
		TreadmillLocomotionVR.SetFloatSetting("CoastMaxSeconds", 0.30)
	ElseIf preset == 1
		TreadmillLocomotionVR.SetFloatSetting("SprintThreshold", 0.75)
		TreadmillLocomotionVR.SetFloatSetting("SprintEnterSeconds", 0.22)
		TreadmillLocomotionVR.SetFloatSetting("SprintExitSeconds", 0.35)
		TreadmillLocomotionVR.SetFloatSetting("SprintCancelSeconds", 0.12)
		TreadmillLocomotionVR.SetFloatSetting("CoastMaxSeconds", 0.25)
	ElseIf preset == 2
		TreadmillLocomotionVR.SetFloatSetting("SprintThreshold", 0.85)
		TreadmillLocomotionVR.SetFloatSetting("SprintEnterSeconds", 0.25)
		TreadmillLocomotionVR.SetFloatSetting("SprintExitSeconds", 0.30)
		TreadmillLocomotionVR.SetFloatSetting("SprintCancelSeconds", 0.10)
		TreadmillLocomotionVR.SetFloatSetting("CoastMaxSeconds", 0.20)
	EndIf

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
