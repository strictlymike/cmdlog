# CMDLOG
Command and output logger for red team use. For use with 32- and 64-bit
`cmd.exe` and `powershell.exe`. For details about internals, see:
* [Snooping on Myself for a Change](http://baileysoriginalirishtech.blogspot.com/2016/02/snooping-on-myself-for-change.html).
* [Snooping Again](http://baileysoriginalirishtech.blogspot.com/2019/07/snooping-again.html).

## Build and Test
================================================================================
Prerequisites:
* Windows Platform SDK or Visual Studio
* Microsoft Detours 4 <http://research.microsoft.com/en-us/projects/detours/>

To Build:
Use `nmake` to build

To Test, run `test.cmd` or execute:

```
withdll.exe /d:cmdlog64.dll `cmd`
```
