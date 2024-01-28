SET WorkingDir=%cd%
SET CStoCppArgs=-std=c++20 -I "%WorkingDir%" -I "C:/Program Files/Mono/include/mono-2.0" -I Test

.\x64\Debug\CStoCPPInteropGenerator.exe Test/ScriptTest.h Test/Generated/RegisterScriptInterface.h TestManaged/Generated ScriptedClassBase %CStoCppArgs%