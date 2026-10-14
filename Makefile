# ============================================================
#  Makefile  –  C99 + C++ (Dear ImGui) 混在ビルド
#
#  使い方:
#    make           → Release ビルド (bin/particle_flow)
#    make DEBUG=1   → Debug ビルド
#    make clean     → ビルド成果物を削除
#    make run       → ビルド後に実行
#    make bench     → 計測モードを実行して bench/results/<LABEL>-native.json に保存
#                     (使い方は bench/README.md)
#
#  依存:
#    Linux:   sudo apt install libsdl2-dev libglew-dev
#    macOS:   brew install sdl2 glew
# ============================================================

CC      := gcc
CXX     := g++
UNAME_S := $(shell uname -s)
.DEFAULT_GOAL := all
TARGET  := bin/particle_flow
SRCDIR  := src
OBJDIR  := obj
THIRDDIR := third_party
IMGDIR  := $(THIRDDIR)/imgui

# ---- コンパイルフラグ ----
COMMON_FLAGS := -Wall -Wextra
DEBUG ?= 0

ifeq ($(DEBUG),1)
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

CFLAGS   := $(COMMON_FLAGS) -std=c99   $(PKG_CFLAGS) -I$(OBJDIR) -MMD -MP
CXXFLAGS := $(COMMON_FLAGS) -std=c++17 $(PKG_CFLAGS) -I$(SRCDIR) -I$(THIRDDIR) -I$(IMGDIR) -MMD -MP
LIBS     := $(PKG_LIBS) -lm

# macOS では OpenGL はライブラリではなく framework としてリンクする。
# Homebrew の glew.pc にはこの指定が含まれないため、明示的に追加する。
ifeq ($(UNAME_S),Darwin)
    LIBS += -framework OpenGL
endif

ifeq ($(OS),Windows_NT)
    LIBS   := -lSDL2 -lSDL2main -lGLEW32 -lopengl32 -lm
    TARGET := bin/particle_flow.exe
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
.PHONY: all clean run FORCE

all: $(TARGET)

# ---- ビルド情報 (bench の結果に git の short sha と未コミットの変更の有無を入れる) ----
# 中身が変わったときだけ書き換えるので、毎回 bench.o が作り直されることはない
BUILD_INFO := $(OBJDIR)/build_info.h
$(BUILD_INFO): FORCE | $(OBJDIR)
	@sha=$$(git rev-parse --short HEAD 2>/dev/null || echo unknown); \
	if git diff --quiet HEAD -- src web Makefile 2>/dev/null; then dirty=0; else dirty=1; fi; \
	printf '#define BUILD_SHA "%s"\n#define BUILD_DIRTY %s\n' "$$sha" "$$dirty" > $@.tmp; \
	if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

$(OBJDIR)/bench.o: $(BUILD_INFO)

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
#  ベンチマーク (bench/README.md)
#
#    make bench LABEL=baseline        → bench/results/baseline-native.json
#    make sim-bench                   → SDL なしの物理だけのマイクロベンチ (ネイティブ)
#    make sim-bench-wasm              → 同じものを wasm にして Node で実行
# ============================================================
LABEL      ?= $(shell git rev-parse --short HEAD 2>/dev/null || echo bench)
BENCH_RUNS ?= 5
BENCH_OUT  := bench/results/$(LABEL)-native.json

.PHONY: bench sim-bench sim-bench-wasm

bench: $(TARGET)
	mkdir -p bench/results
	$(TARGET) --bench --bench-runs=$(BENCH_RUNS) --bench-label=$(LABEL) --bench-out=$(BENCH_OUT)

SIM_BENCH_SRCS := bench/sim_bench.c $(SRCDIR)/particle.c $(SRCDIR)/bench_core.c

bin/sim_bench: $(SIM_BENCH_SRCS) $(SRCDIR)/particle.h $(SRCDIR)/bench_core.h $(BUILD_INFO) | bin
	$(CC) $(COMMON_FLAGS) -std=c99 -I$(SRCDIR) -I$(OBJDIR) -o $@ $(SIM_BENCH_SRCS) -lm

sim-bench: bin/sim_bench
	mkdir -p bench/results
	./bin/sim_bench --label=$(LABEL) --runs=$(BENCH_RUNS) --out=bench/results/$(LABEL)-native-sim.json

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
    -I$(SRCDIR) -I$(THIRDDIR) -I$(IMGDIR)

WASM_CFLAGS   := $(WASM_COMMON) -std=c99 -I$(OBJDIR)
WASM_CXXFLAGS := $(WASM_COMMON) -std=c++17

# web/shell.html を元に bin/index.html を作る(キャンバスを画面いっぱいにするページ)
WASM_SHELL := web/shell.html

# リンク時にも -O2 が要る(無いとデバッグ版のシステムライブラリが使われ、JS も縮小されない)
WASM_LDFLAGS := \
    -O2 \
    -sUSE_SDL=2 \
    -sUSE_WEBGL2=1 \
    -sFULL_ES3=1 \
    -sWASM=1 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=67108864 \
    --shell-file $(WASM_SHELL)

# 計測モードの結果をページに出すのに使う (bench.c の EM_JS)
WASM_LDFLAGS += -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$$UTF8ToString

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

$(WASM_TARGET): $(WASM_ALL_OBJS) $(WASM_SHELL) | bin
	$(EMCXX) -o $@ $(WASM_ALL_OBJS) $(WASM_LDFLAGS)

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

$(WASMDIR)/bench.o: $(BUILD_INFO)

$(WASMDIR):
	mkdir -p $(WASMDIR)

# sim_bench の wasm 版 (Node で実行する)。フラグはアプリの wasm と同じ WASM_COMMON
bin/sim_bench.js: $(SIM_BENCH_SRCS) $(SRCDIR)/particle.h $(SRCDIR)/bench_core.h $(BUILD_INFO) | bin
	$(EMCC) $(WASM_CFLAGS) -o $@ $(SIM_BENCH_SRCS) \
	    $(filter-out -sUSE_SDL=2 -sUSE_WEBGL2=1 -sFULL_ES3=1 --shell-file $(WASM_SHELL),$(WASM_LDFLAGS)) \
	    -sENVIRONMENT=node -sNODERAWFS=1

sim-bench-wasm: bin/sim_bench.js
	mkdir -p bench/results
	node bin/sim_bench.js --label=$(LABEL) --runs=$(BENCH_RUNS) --out=bench/results/$(LABEL)-wasm-sim.json

wasm-clean:
	rm -rf $(WASMDIR) $(WASM_TARGET) bin/index.js bin/index.wasm
