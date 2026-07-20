ScriptName SKI_ConfigBase Extends Quest

String Property ModName Auto
String[] Property Pages Auto

Int Property TOP_TO_BOTTOM = 2 AutoReadOnly

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

Int Function GetVersion()
	Return 1
EndFunction

Function SetCursorFillMode(Int fillMode)
EndFunction

Function SetInfoText(String text)
EndFunction

Function ForcePageReset()
EndFunction

Int Function AddHeaderOption(String text, Int flags = 0)
	Return 0
EndFunction

Int Function AddTextOption(String text, String value, Int flags = 0)
	Return 0
EndFunction

Int Function AddToggleOption(String text, Bool checked, Int flags = 0)
	Return 0
EndFunction

Int Function AddSliderOption(String text, Float value, String formatString = "{0}", Int flags = 0)
	Return 0
EndFunction

Function SetToggleOptionValue(Int option, Bool checked, Bool noUpdate = False)
EndFunction

Function SetSliderOptionValue(Int option, Float value, String formatString = "{0}", Bool noUpdate = False)
EndFunction

Function SetSliderDialogStartValue(Float value)
EndFunction

Function SetSliderDialogDefaultValue(Float value)
EndFunction

Function SetSliderDialogRange(Float minValue, Float maxValue)
EndFunction

Function SetSliderDialogInterval(Float value)
EndFunction
