# GNU Make project makefile autogenerated by Premake
ifndef config
  config=release
endif

ifndef verbose
  SILENT = @
endif

CC = gcc
CXX = g++
AR = ar

ifndef RESCOMP
  ifdef WINDRES
    RESCOMP = $(WINDRES)
  else
    RESCOMP = windres
  endif
endif

ifeq ($(config),release)
  OBJDIR     = ../../../../Output/Targets/Linux-x86-32/Release/obj/quake-gles2
  TARGETDIR  = ../../../../Output/Targets/Linux-x86-32/Release/bin
  TARGET     = $(TARGETDIR)/quake-gles2
  DEFINES   += -D_GNU_SOURCE=1 -DEGLW_GLES2
  INCLUDES  += -I../../../../../../Engine/External/include -I../../../../Sources -I../../../../../../Engine/Sources/Compatibility -I../../../../../../Engine/Sources/Compatibility/OpenGLES/Includes
  ALL_CPPFLAGS  += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS    += $(CFLAGS) $(ALL_CPPFLAGS) $(ARCH) -ffast-math -Wall -Wextra -O2 -std=c99 -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-switch -Wno-missing-field-initializers -fPIC -fvisibility=hidden
  ALL_CXXFLAGS  += $(CXXFLAGS) $(ALL_CFLAGS)
  ALL_RESFLAGS  += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  ALL_LDFLAGS   += $(LDFLAGS) -L../../../../Output/Targets/Linux-x86-32/Release/lib -L. -s
  LDDEPS    +=
  LIBS      += $(LDDEPS) -lm -ldl -lGLESv2 -lEGL -lSDL2main -lSDL2 -lX11
  LINKCMD    = $(CC) -o $(TARGET) $(OBJECTS) $(RESOURCES) $(ARCH) $(ALL_LDFLAGS) $(LIBS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
endif

ifeq ($(config),debug)
  OBJDIR     = ../../../../Output/Targets/Linux-x86-32/Debug/obj/quake-gles2
  TARGETDIR  = ../../../../Output/Targets/Linux-x86-32/Debug/bin
  TARGET     = $(TARGETDIR)/quake-gles2
  DEFINES   += -D_GNU_SOURCE=1 -DEGLW_GLES2
  INCLUDES  += -I../../../../../../Engine/External/include -I../../../../Sources -I../../../../../../Engine/Sources/Compatibility -I../../../../../../Engine/Sources/Compatibility/OpenGLES/Includes
  ALL_CPPFLAGS  += $(CPPFLAGS) -MMD -MP $(DEFINES) $(INCLUDES)
  ALL_CFLAGS    += $(CFLAGS) $(ALL_CPPFLAGS) $(ARCH) -ffast-math -Wall -Wextra -g -std=c99 -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-switch -Wno-missing-field-initializers -fPIC -fvisibility=hidden
  ALL_CXXFLAGS  += $(CXXFLAGS) $(ALL_CFLAGS)
  ALL_RESFLAGS  += $(RESFLAGS) $(DEFINES) $(INCLUDES)
  ALL_LDFLAGS   += $(LDFLAGS) -L../../../../Output/Targets/Linux-x86-32/Debug/lib -L.
  LDDEPS    +=
  LIBS      += $(LDDEPS) -lm -ldl -lGLESv2 -lEGL -lSDL2main -lSDL2 -lX11
  LINKCMD    = $(CC) -o $(TARGET) $(OBJECTS) $(RESOURCES) $(ARCH) $(ALL_LDFLAGS) $(LIBS)
  define PREBUILDCMDS
  endef
  define PRELINKCMDS
  endef
  define POSTBUILDCMDS
  endef
endif

OBJECTS := \
	$(OBJDIR)/cl_demo.o \
	$(OBJDIR)/cl_input.o \
	$(OBJDIR)/cl_main.o \
	$(OBJDIR)/cl_parse.o \
	$(OBJDIR)/cl_tent.o \
	$(OBJDIR)/console.o \
	$(OBJDIR)/keys.o \
	$(OBJDIR)/menu.o \
	$(OBJDIR)/menu_gameoptions.o \
	$(OBJDIR)/menu_help.o \
	$(OBJDIR)/menu_keys.o \
	$(OBJDIR)/menu_lanconfig.o \
	$(OBJDIR)/menu_loadsave.o \
	$(OBJDIR)/menu_main.o \
	$(OBJDIR)/menu_multiplayer.o \
	$(OBJDIR)/menu_network.o \
	$(OBJDIR)/menu_options.o \
	$(OBJDIR)/menu_quit.o \
	$(OBJDIR)/menu_search.o \
	$(OBJDIR)/menu_serverlist.o \
	$(OBJDIR)/menu_setup.o \
	$(OBJDIR)/menu_singleplayer.o \
	$(OBJDIR)/sbar.o \
	$(OBJDIR)/view.o \
	$(OBJDIR)/cmd.o \
	$(OBJDIR)/common.o \
	$(OBJDIR)/crc.o \
	$(OBJDIR)/cvar.o \
	$(OBJDIR)/mathlib.o \
	$(OBJDIR)/wad.o \
	$(OBJDIR)/zone.o \
	$(OBJDIR)/host.o \
	$(OBJDIR)/host_cmd.o \
	$(OBJDIR)/net.o \
	$(OBJDIR)/net_dgrm.o \
	$(OBJDIR)/net_loop.o \
	$(OBJDIR)/net_main.o \
	$(OBJDIR)/net_udp.o \
	$(OBJDIR)/net_vcr.o \
	$(OBJDIR)/chase.o \
	$(OBJDIR)/r_draw.o \
	$(OBJDIR)/r_main.o \
	$(OBJDIR)/r_mesh.o \
	$(OBJDIR)/r_model.o \
	$(OBJDIR)/r_part.o \
	$(OBJDIR)/r_screen.o \
	$(OBJDIR)/pr_cmds.o \
	$(OBJDIR)/pr_edict.o \
	$(OBJDIR)/pr_exec.o \
	$(OBJDIR)/sv_main.o \
	$(OBJDIR)/sv_move.o \
	$(OBJDIR)/sv_phys.o \
	$(OBJDIR)/sv_user.o \
	$(OBJDIR)/world.o \
	$(OBJDIR)/sound.o \
	$(OBJDIR)/audio_sdl.o \
	$(OBJDIR)/cd_sdl.o \
	$(OBJDIR)/input_sdl.o \
	$(OBJDIR)/system_sdl.o \
	$(OBJDIR)/video_sdl.o \
	$(OBJDIR)/EGLWrapper.o \
	$(OBJDIR)/OpenGLWrapper.o \
	$(OBJDIR)/SDLWrapper.o \

RESOURCES := \

SHELLTYPE := msdos
ifeq (,$(ComSpec)$(COMSPEC))
  SHELLTYPE := posix
endif
ifeq (/bin,$(findstring /bin,$(SHELL)))
  SHELLTYPE := posix
endif

.PHONY: clean prebuild prelink

all: $(TARGETDIR) $(OBJDIR) prebuild prelink $(TARGET)
	@:

$(TARGET): $(GCH) $(OBJECTS) $(LDDEPS) $(RESOURCES)
	@echo Linking quake-gles2
	$(SILENT) $(LINKCMD)
	$(POSTBUILDCMDS)

$(TARGETDIR):
	@echo Creating $(TARGETDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(TARGETDIR)
else
	$(SILENT) mkdir $(subst /,\\,$(TARGETDIR))
endif

$(OBJDIR):
	@echo Creating $(OBJDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(OBJDIR)
else
	$(SILENT) mkdir $(subst /,\\,$(OBJDIR))
endif

clean:
	@echo Cleaning quake-gles2
ifeq (posix,$(SHELLTYPE))
	$(SILENT) rm -f  $(TARGET)
	$(SILENT) rm -rf $(OBJDIR)
else
	$(SILENT) if exist $(subst /,\\,$(TARGET)) del $(subst /,\\,$(TARGET))
	$(SILENT) if exist $(subst /,\\,$(OBJDIR)) rmdir /s /q $(subst /,\\,$(OBJDIR))
endif

prebuild:
	$(PREBUILDCMDS)

prelink:
	$(PRELINKCMDS)

ifneq (,$(PCH))
$(GCH): $(PCH)
	@echo $(notdir $<)
	$(SILENT) $(CC) -x c-header $(ALL_CFLAGS) -MMD -MP $(DEFINES) $(INCLUDES) -o "$@" -MF "$(@:%.gch=%.d)" -c "$<"
endif

$(OBJDIR)/cl_demo.o: ../../../../Sources/Client/cl_demo.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/cl_input.o: ../../../../Sources/Client/cl_input.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/cl_main.o: ../../../../Sources/Client/cl_main.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/cl_parse.o: ../../../../Sources/Client/cl_parse.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/cl_tent.o: ../../../../Sources/Client/cl_tent.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/console.o: ../../../../Sources/Client/console.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/keys.o: ../../../../Sources/Client/keys.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu.o: ../../../../Sources/Client/menu.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_gameoptions.o: ../../../../Sources/Client/menu_gameoptions.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_help.o: ../../../../Sources/Client/menu_help.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_keys.o: ../../../../Sources/Client/menu_keys.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_lanconfig.o: ../../../../Sources/Client/menu_lanconfig.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_loadsave.o: ../../../../Sources/Client/menu_loadsave.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_main.o: ../../../../Sources/Client/menu_main.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_multiplayer.o: ../../../../Sources/Client/menu_multiplayer.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_network.o: ../../../../Sources/Client/menu_network.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_options.o: ../../../../Sources/Client/menu_options.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_quit.o: ../../../../Sources/Client/menu_quit.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_search.o: ../../../../Sources/Client/menu_search.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_serverlist.o: ../../../../Sources/Client/menu_serverlist.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_setup.o: ../../../../Sources/Client/menu_setup.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/menu_singleplayer.o: ../../../../Sources/Client/menu_singleplayer.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/sbar.o: ../../../../Sources/Client/sbar.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/view.o: ../../../../Sources/Client/view.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/cmd.o: ../../../../Sources/Common/cmd.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/common.o: ../../../../Sources/Common/common.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/crc.o: ../../../../Sources/Common/crc.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/cvar.o: ../../../../Sources/Common/cvar.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/mathlib.o: ../../../../Sources/Common/mathlib.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/wad.o: ../../../../Sources/Common/wad.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/zone.o: ../../../../Sources/Common/zone.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/host.o: ../../../../Sources/Host/host.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/host_cmd.o: ../../../../Sources/Host/host_cmd.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/net.o: ../../../../Sources/Networking/net.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/net_dgrm.o: ../../../../Sources/Networking/net_dgrm.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/net_loop.o: ../../../../Sources/Networking/net_loop.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/net_main.o: ../../../../Sources/Networking/net_main.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/net_udp.o: ../../../../Sources/Networking/net_udp.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/net_vcr.o: ../../../../Sources/Networking/net_vcr.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/chase.o: ../../../../Sources/Rendering/chase.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/r_draw.o: ../../../../Sources/Rendering/r_draw.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/r_main.o: ../../../../Sources/Rendering/r_main.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/r_mesh.o: ../../../../Sources/Rendering/r_mesh.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/r_model.o: ../../../../Sources/Rendering/r_model.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/r_part.o: ../../../../Sources/Rendering/r_part.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/r_screen.o: ../../../../Sources/Rendering/r_screen.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/pr_cmds.o: ../../../../Sources/Scripting/pr_cmds.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/pr_edict.o: ../../../../Sources/Scripting/pr_edict.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/pr_exec.o: ../../../../Sources/Scripting/pr_exec.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/sv_main.o: ../../../../Sources/Server/sv_main.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/sv_move.o: ../../../../Sources/Server/sv_move.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/sv_phys.o: ../../../../Sources/Server/sv_phys.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/sv_user.o: ../../../../Sources/Server/sv_user.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/world.o: ../../../../Sources/Server/world.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/sound.o: ../../../../Sources/Sound/sound.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/audio_sdl.o: ../../../../Sources/System/audio_sdl.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/cd_sdl.o: ../../../../Sources/System/cd_sdl.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/input_sdl.o: ../../../../Sources/System/input_sdl.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/system_sdl.o: ../../../../Sources/System/system_sdl.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/video_sdl.o: ../../../../Sources/System/video_sdl.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/EGLWrapper.o: ../../../../../../Engine/Sources/Compatibility/OpenGLES/EGLWrapper.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/OpenGLWrapper.o: ../../../../../../Engine/Sources/Compatibility/OpenGLES/OpenGLWrapper.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

$(OBJDIR)/SDLWrapper.o: ../../../../../../Engine/Sources/Compatibility/SDL/SDLWrapper.c
	@echo $(notdir $<)
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF $(@:%.o=%.d) -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(OBJDIR)/$(notdir $(PCH)).d
endif
