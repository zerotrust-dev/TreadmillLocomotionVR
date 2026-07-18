ScriptName SKI_ConfigBase Extends Quest

Int Property TOP_TO_BOTTOM = 0 AutoReadOnly
String Property ModName Auto
String[] Property Pages Auto

Int Function GetVersion()
	Return 1
EndFunction

Event OnConfigInit()
EndEvent

Event OnConfigOpen()
EndEvent

Event OnConfigClose()
EndEvent

Event OnPageReset(String page)
EndEvent

Event OnOptionHighlight(Int option)
EndEvent

Event OnOptionSelect(Int option)
EndEvent

Event OnOptionSliderOpen(Int option)
EndEvent

Event OnOptionSliderAccept(Int option, Float value)
EndEvent

Function SetCursorFillMode(Int mode)
EndFunction

Function SetInfoText(String text)
EndFunction

Int Function AddHeaderOption(String text)
	Return 0
EndFunction

Int Function AddToggleOption(String text, Bool checked)
	Return 0
EndFunction

Int Function AddSliderOption(String text, Float value, String formatString)
	Return 0
EndFunction

Int Function AddTextOption(String text, String value)
	Return 0
EndFunction

Function SetToggleOptionValue(Int option, Bool checked)
EndFunction

Function SetSliderOptionValue(Int option, Float value, String formatString)
EndFunction

Function SetSliderDialogStartValue(Float value)
EndFunction

Function SetSliderDialogDefaultValue(Float value)
EndFunction

Function SetSliderDialogRange(Float minValue, Float maxValue)
EndFunction

Function SetSliderDialogInterval(Float interval)
EndFunction

Function ForcePageReset()
EndFunction
