# ============================================================
#  Makefile  –  C99 + C++ (Dear ImGui) 混在ビルド
#
#  使い方:
#    make           → Release ビルド (bin/particle_sim)
#    make DEBUG=1   → Debug ビルド
#    make clean     → ビルド成果物を削除
#    make run       → ビルド後に実行
#
#  依存:
#    Linux:   sudo apt install libsdl2-dev libglew-dev
#    macOS:   brew install sdl2 glew
# ============================================================

CC      := gcc
CXX     := g++
TARGET  := bin/particle_sim
SRCDIR  := src
OBJDIR  := obj
IMGDIR  := src/imgui

# ---- コンパイルフラグ ----
COMMON_FLAGS := -Wall -Wextra

ifdef DEBUG
    COMMON_FLAGS += -g -O0 -DDEBUG
else
    COMMON_FLAGS += -O3 -march=native -ffast-math -funroll-loops -DNDEBUG
endif

# ---- リンクフラグ ----
PKG_CFLAGS := $(shell pkg-config --cflags sdl2 glew 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs   sdl2 glew 2>/dev/null)

ifeq ($(PKG_LIBS),)
    PKG_CFLAGS :=
    PKG_LIBS   := -lSDL2 -lSDL2main -lGLEW -lGL -lm
endif

CFLAGS   := $(COMMON_FLAGS) -std=c99   $(PKG_CFLAGS) -MMD -MP
CXXFLAGS := $(COMMON_FLAGS) -std=c++17 $(PKG_CFLAGS) -I$(SRCDIR) -I$(IMGDIR) -MMD -MP
LIBS     := $(PKG_LIBS) -lm

ifeq ($(OS),Windows_NT)
    LIBS   := -lSDL2 -lSDL2main -lGLEW32 -lopengl32 -lm
    TARGET := bin/particle_sim.exe
endif

# ---- ソース → オブジェクト ----
C_SRCS := $(wildcard $(SRCDIR)/*.c)
C_OBJS := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(C_SRCS))

# gui.cpp (プロジェクト C++ ファイル)
CPP_OBJS := $(OBJDIR)/gui.o

# ImGui ライブラリ
IMGUI_OBJS := \
    $(OBJDIR)/im_imgui.o \
    $(OBJDIR)/im_draw.o \
    $(OBJDIR)/im_tables.o \
    $(OBJDIR)/im_widgets.o \
    $(OBJDIR)/im_sdl2.o \
    $(OBJDIR)/im_opengl3.o

ALL_OBJS := $(C_OBJS) $(CPP_OBJS) $(IMGUI_OBJS)

# -MMD が生成した依存ファイル (.d) を自動インクルード
DEPS := $(ALL_OBJS:.o=.d)
-include $(DEPS)

# ---- ルール ----
.PHONY: all clean run

all: $(TARGET)

# リンクは g++ で行う (C++ ランタイムのため)
$(TARGET): $(ALL_OBJS) | bin
	$(CXX) -o $@ $^ $(LIBS)

# C ファイル
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# gui.cpp
$(OBJDIR)/gui.o: $(SRCDIR)/gui.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ImGui コアファイル
$(OBJDIR)/im_imgui.o: $(IMGDIR)/imgui.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/im_draw.o: $(IMGDIR)/imgui_draw.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/im_tables.o: $(IMGDIR)/imgui_tables.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/im_widgets.o: $(IMGDIR)/imgui_widgets.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/im_sdl2.o: $(IMGDIR)/imgui_impl_sdl2.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/im_opengl3.o: $(IMGDIR)/imgui_impl_opengl3.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin:
	mkdir -p bin

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) bin

run: all
	$(TARGET)

# ============================================================
#  WebAssembly ターゲット (Emscripten)
#
#  使い方:
#    source ~/emsdk/emsdk_env.sh
#    make wasm
#    python3 -m http.server -d bin   # ブラウザで localhost:8000/index.html
# ============================================================
EMCC    := emcc
EMCXX   := em++
WASMDIR := $(OBJDIR)/wasm
WASM_TARGET := bin/index.html

WASM_COMMON := \
    -O2 \
    -DIMGUI_IMPL_OPENGL_ES3 \
    -sUSE_SDL=2 \
    -I$(SRCDIR) -I$(IMGDIR)

WASM_CFLAGS   := $(WASM_COMMON) -std=c99
WASM_CXXFLAGS := $(WASM_COMMON) -std=c++17

WASM_LDFLAGS := \
    -sUSE_SDL=2 \
    -sUSE_WEBGL2=1 \
    -sFULL_ES3=1 \
    -sWASM=1 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=67108864

WASM_C_SRCS  := $(wildcard $(SRCDIR)/*.c)
WASM_C_OBJS  := $(patsubst $(SRCDIR)/%.c, $(WASMDIR)/%.o, $(WASM_C_SRCS))

WASM_IMGUI_OBJS := \
    $(WASMDIR)/im_imgui.o \
    $(WASMDIR)/im_draw.o \
    $(WASMDIR)/im_tables.o \
    $(WASMDIR)/im_widgets.o \
    $(WASMDIR)/im_sdl2.o \
    $(WASMDIR)/im_opengl3.o

WASM_ALL_OBJS := $(WASM_C_OBJS) $(WASMDIR)/gui.o $(WASM_IMGUI_OBJS)

.PHONY: wasm wasm-clean

wasm: $(WASM_TARGET)

$(WASM_TARGET): $(WASM_ALL_OBJS) | bin
	$(EMCXX) -o $@ $^ $(WASM_LDFLAGS)

$(WASMDIR)/%.o: $(SRCDIR)/%.c | $(WASMDIR)
	$(EMCC) $(WASM_CFLAGS) -c -o $@ $<

$(WASMDIR)/gui.o: $(SRCDIR)/gui.cpp | $(WASMDIR)
	$(EMCXX) $(WASM_CXXFLAGS) -c -o $@ $<

$(WASMDIR)/im_imgui.o: $(IMGDIR)/imgui.cpp | $(WASMDIR)
	$(EMCXX) $(WASM_CXXFLAGS) -c -o $@ $<

$(WASMDIR)/im_draw.o: $(IMGDIR)/imgui_draw.cpp | $(WASMDIR)
	$(EMCXX) $(WASM_CXXFLAGS) -c -o $@ $<

$(WASMDIR)/im_tables.o: $(IMGDIR)/imgui_tables.cpp | $(WASMDIR)
	$(EMCXX) $(WASM_CXXFLAGS) -c -o $@ $<

$(WASMDIR)/im_widgets.o: $(IMGDIR)/imgui_widgets.cpp | $(WASMDIR)
	$(EMCXX) $(WASM_CXXFLAGS) -c -o $@ $<

$(WASMDIR)/im_sdl2.o: $(IMGDIR)/imgui_impl_sdl2.cpp | $(WASMDIR)
	$(EMCXX) $(WASM_CXXFLAGS) -c -o $@ $<

$(WASMDIR)/im_opengl3.o: $(IMGDIR)/imgui_impl_opengl3.cpp | $(WASMDIR)
	$(EMCXX) $(WASM_CXXFLAGS) -c -o $@ $<

$(WASMDIR):
	mkdir -p $(WASMDIR)

wasm-clean:
	rm -rf $(WASMDIR) $(WASM_TARGET) bin/index.js bin/index.wasm
