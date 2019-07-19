##############################################################################
##
##  API Extention to Measure time slept.
##
##  Microsoft Research Detours Package, Version 3.0.
##
##  Copyright (c) Microsoft Corporation.  All rights reserved.
##

!include ..\common.mak

!IF $(DETOURS_BITS)==32
PYINCD=C:\Program Files (x86)\Python37-32\include
PYLIBD=C:\Program Files (x86)\Python37-32\libs
!ELSE
PYINCD=C:\Program Files\Python37\include
PYLIBD=C:\Program Files\Python37\libs
!ENDIF

##############################################################################

python = N

modname=cmdlog

CFLAGS = $(CFLAGS) "/I$(PYINCD)" /EHsc /Oy-
LINKFLAGS = $(LINKFLAGS) "/LIBPATH:$(PYLIBD)"

# Unless you enjoy reading this:
#
# detours.lib(creatwth.obj) : error LNK2019: unresolved external symbol _vsnwpr
# intf referenced in function "long __cdecl StringVPrintfWorkerW(wchar_t *,unsi
# gned __int64,unsigned __int64 *,wchar_t const *,char *)" (?StringVPrintfWorke
# rW@@YAJPEA_W_KPEA_KPEB_WPEAD@Z)
#
# Do this for VS2017 and recent Windows Platform SDKs:
LIBS = $(LIBS) legacy_stdio_definitions.lib

all: dirs \
    $(BIND)\$(modname)$(DETOURS_BITS).dll \
    \
!IF $(DETOURS_SOURCE_BROWSING)==1
    $(OBJD)\$(modname)$(DETOURS_BITS).bsc \
!ENDIF
    option

##############################################################################

dirs:
    @if not exist $(BIND) mkdir $(BIND) && echo.   Created $(BIND)
    @if not exist $(OBJD) mkdir $(OBJD) && echo.   Created $(OBJD)

$(OBJD)\$(modname).obj : $(modname).cpp

$(OBJD)\logging.obj : logging.cpp

$(OBJD)\$(modname).res : $(modname).rc

# Regarding DetourCreateProcessWithDlls,@1,NONAME, see documentation for
# DetourCreateProcessWithDlls
$(BIND)\$(modname)$(DETOURS_BITS).dll $(BIND)\$(modname)$(DETOURS_BITS).lib: \
        $(OBJD)\$(modname).obj $(OBJD)\logging.obj \
		$(OBJD)\$(modname).res $(DEPS)
    cl /LD $(CFLAGS) /Fe$@ /Fd$(@R).pdb \
        $(OBJD)\$(modname).obj $(OBJD)\logging.obj \
		$(OBJD)\$(modname).res \
        /link $(LINKFLAGS) /incremental:no /subsystem:console \
        /export:DetourFinishHelperProcess,@1,NONAME \
        /export:LogReadConsoleW \
        /export:LogWriteConsoleW \
        $(LIBS)

$(OBJD)\$(modname)$(DETOURS_BITS).bsc : $(OBJD)\$(modname).obj \
		$(OBJD)\logging.obj
    bscmake /v /n /o $@ $(OBJD)\$(modname).sbr

##############################################################################

clean:
    -del *~ 2>nul
    -del $(BIND)\$(modname)*.* 2>nul
    -rmdir /q /s $(OBJD) 2>nul

realclean: clean
    -rmdir /q /s $(OBJDS) 2>nul

############################################### Install non-bit-size binaries.

!IF "$(DETOURS_OPTION_PROCESSOR)" != ""

$(OPTD)\$(modname)$(DETOURS_OPTION_BITS).dll:
$(OPTD)\$(modname)$(DETOURS_OPTION_BITS).pdb:

$(BIND)\$(modname)$(DETOURS_OPTION_BITS).dll : $(OPTD)\$(modname)$(DETOURS_OPTION_BITS).dll
    @if exist $? copy /y $? $(BIND) >nul && echo $@ copied from $(DETOURS_OPTION_PROCESSOR).
$(BIND)\$(modname)$(DETOURS_OPTION_BITS).pdb : $(OPTD)\$(modname)$(DETOURS_OPTION_BITS).pdb
    @if exist $? copy /y $? $(BIND) >nul && echo $@ copied from $(DETOURS_OPTION_PROCESSOR).

option: \
    $(BIND)\$(modname)$(DETOURS_OPTION_BITS).dll \
    $(BIND)\$(modname)$(DETOURS_OPTION_BITS).pdb \

!ELSE

option:

!ENDIF

################################################################# End of File.
