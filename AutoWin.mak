# This file is designed to abstract out differences between versions of
# Microsoft Visual C++ and common compilation options for that
# environment.  It is designed to be independent of the package that it
# ships with.  Please don't attempt to introduce application-specific
# options here; keep these general.
#
# I will attempt to support versions of Microsoft Visual C++ right back
# to Optimizing Compiler version 8 (NMAKE 1.1) in this file.  That's
# pre-Visual C++ 2.0.

!include <ntwin32.mak>

#
# Supported options:
# MSC_VER =     n - Specify the version of Microsoft C++ - NOT Visual C++.
#               2.0 is 9, 4.0 is 10, 5.0 is 11, 6.0 is 12, 2002/2003 is 13,
#               Whidbey is 14, etc.  Default is (currently) 12.
!IFNDEF MSC_VER
MSC_VER = 12
!ENDIF

# MSC_ED =      std,pro,free - Specify the edition of Microsoft Visual C++;
#               use std for Standard Edition, pro for Professional Edition,
#               and free for Visual C++ Toolkit (free download.)  Default
#               is pro for MSC_VER <= 12, free for MSC_VER > 12.
!IFNDEF MSC_ED
!IF $(MSC_VER) > 12
MSC_ED=free
!ELSE
MSC_ED=pro
!ENDIF
!ENDIF

# CLIB_SHARED = 0 - Link statically against your C library.  Generally not
#               a good idea, but you *must* have this to compile with the
#               free Visual C++ Toolkit; and if you have newer compilers,
#               your customers might not have that version of the shared
#               C library installed, so using it is a bad idea.  Default
#               is 1 for MSC_VER <= 12; 0 afterwards.
!IFNDEF CLIB_SHARED
!IF $(MSC_VER) < 13
CLIB_SHARED = 1
!ELSE
CLIB_SHARED = 0
!ENDIF
!ENDIF

# DEBUG      = 1 - Enable debugging support.  Default is 0.
!IFNDEF DEBUG
DEBUG = 0
!ENDIF

# RELOC      = 1 - Enable relocation information in executables.  This
#              results in larger binaries, but they are Win32s (Windows
#              3.1) compatible.  Default is 0.
!IFNDEF RELOC
RELOC = 0
!ENDIF

# PC_HEADERS = 1 - Enable pre-compiled headers.  Depending on the
#              application, this may reduce build times, and will be handy
#              if you're developing that application.  The default is 0.
!IFNDEF PC_HEADERS
PC_HEADERS = 0
!ENDIF

# WITH_3D    = 1 - Enable linking against ctl3d32.lib.  This is only used
#              on old versions of MSC to get 3D appearence; things already
#              look this way on newer versions.  The default is 0 except
#              on MSC 9 (Visual C++ 2.0).  This is the only version that
#              shipped with a default non-3D appearence.
!IFNDEF WITH_3D
!IF $(MSC_VER) == 9
WITH_3D = 1
!ELSE
WITH_3D = 0
!ENDIF
!ENDIF

BASE_CFLAGS=-W3 -nologo -D_WIN32_
BASE_LDFLAGS=
BASE_LIBS=kernel32.lib
BASE_ILFLAGS=

# Now we start generating everything.  Don't change lightly.

# Crazy variable-name cycling necessary for *very* old NMAKE's.
# These things didn't do direct substitution., ie CFLAGS=$(CFLAGS) is a
# circular reference.
!IF "$(CLIB_SHARED)" == "1"
CLIBFLAG = -MD
!IF $(DEBUG) == 1
!IF $(MSC_VER) >= 10
!IF "$(MSC_ED)" != "free"
CLIBFLAG = -MDd
!ENDIF
!ENDIF
!ENDIF
CLIB_SHARED=1
!ELSE
CLIBFLAG = -MT
!IF $(DEBUG) == 1
!IF $(MSC_VER) >= 10
!IF "$(MSC_ED)" != "free"
CLIBFLAG = -MTd
!ENDIF
!ENDIF
!ENDIF
CLIB_SHARED=0
!ENDIF

!IF "$(DEBUG)" == "1"
CDEBUGFLAG=-Zi
!IF $(MSC_VER) <= 10
LDEBUGFLAG=-DEBUG:full -DEBUGTYPE:both
!ELSE
LDEBUGFLAG=-DEBUG
!ENDIF
DEBUG=1
!ELSE
!IF "$(MSC_ED)" == "std"
# Standard editions have no optimization.  Free ones do.  Go figure.
CDEBUGFLAG=-Gy
!ELSE
CDEBUGFLAG=-Ox -Gy
!ENDIF
LDEBUGFLAG=
DEBUG=0
!ENDIF

!IF $(MSC_VER) < 13
CVERFLAGS=-Gf
!ELSE
CVERFLAGS=-GF
!ENDIF

!IF $(MSC_VER) == 8
# Picked up from compiler option on newer compilers
!IF "$(CLIB_SHARED)" == "1"
CVERLIBS=crtdll.lib
!ELSE 
CVERLIBS=libcmt.lib
!ENDIF #CLIB_SHARED
LVERFLAGS=-base:0x400000
!ELSE
LVERFLAGS=-nologo
ILVERFLAGS=/nologo
!ENDIF #MSC_VER

!IF "$(PC_HEADERS)" == "1"
CPCHFLAGS=-YX
!ELSE
CPCHFLAGS=
!ENDIF

!IF "$(RELOC)" == "1"
LRELOCFLAGS=
!ELSE
LRELOCFLAGS=-fixed
!ENDIF

!IF "$(WITH_3D)" == "1"
3D_LIBS=ctl3d32.lib
3D_CFLAGS=-DWITH_3D
3D_RCFLAGS=-d WITH_3D
!ELSE
3D_LIBS=
3D_CFLAGS=
3D_RCFLAGS=
!ENDIF

BSCMAKE=REM You don't have bscmake -- 
!IF $(MSC_VER) > 8
!IF "$(MSC_ED)" != "free"
BSCMAKE=bscmake -nologo
!ENDIF
!ENDIF


CFLAGS=$(BASE_CFLAGS) $(CLIBFLAG) $(CDEBUGFLAG) $(CVERFLAGS) $(CPCHFLAGS) $(3D_CFLAGS)
LDFLAGS=$(BASE_LDFLAGS) $(LDEBUGFLAG) $(LRELOCFLAGS) $(LVERFLAGS)
LIBS=$(BASE_LIBS) $(CVERLIBS) $(3D_LIBS)
ILFLAGS=$(BASE_ILFLAGS) $(ILVERFLAGS)
RCFLAGS=$(BASE_RCFLAGS) $(3D_RCFLAGS)

# The only issue here is to make sure DLL's are *never* fixed.
DLL_CFLAGS=$(CFLAGS)
DLL_LIBS=$(LIBS)
DLL_ILFLAGS=$(ILFLAGS)
DLL_RCFLAGS=$(RCFLAGS)
DLL_LDFLAGS=$(BASE_LDFLAGS) $(LDEBUGFLAG) $(LVERFLAGS)

BUILD=$(MAKE) -nologo -f Makefile.w32

w32bcfg.mak:
	@echo Regenerating Win32 Build configuration settings...
	@echo CFLAGS=$(CFLAGS) >w32bcfg.mak
	@echo LDFLAGS=$(LDFLAGS) >>w32bcfg.mak
	@echo ILFLAGS=$(ILFLAGS) >>w32bcfg.mak
	@echo RCFLAGS=$(RCFLAGS) >>w32bcfg.mak
	@echo LIBS=$(LIBS) >>w32bcfg.mak
	@echo DLL_CFLAGS=$(DLL_CFLAGS) >>w32bcfg.mak
	@echo DLL_LDFLAGS=$(DLL_LDFLAGS) >>w32bcfg.mak
	@echo DLL_ILFLAGS=$(DLL_ILFLAGS) >>w32bcfg.mak
	@echo DLL_RCFLAGS=$(DLL_RCFLAGS) >>w32bcfg.mak
	@echo DLL_LIBS=$(DLL_LIBS) >>w32bcfg.mak
	@echo BSCMAKE=$(BSCMAKE) >>w32bcfg.mak
	@echo MSC_VER=$(MSC_VER) >>w32bcfg.mak
	@echo MSC_ED=$(MSC_ED) >>w32bcfg.mak
	@echo CLIB_SHARED=$(CLIB_SHARED) >>w32bcfg.mak
	@echo DEBUG=$(DEBUG) >>w32bcfg.mak
	@echo PC_HEADERS=$(PC_HEADERS) >>w32bcfg.mak
	@echo WITH_3D=$(WITH_3D) >>w32bcfg.mak
	@echo BUILD=$(BUILD) >>w32bcfg.mak
	@echo Current build settings:
	@type w32bcfg.mak
	
