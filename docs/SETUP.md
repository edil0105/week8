# Ортаны баптау

## 1. Windows

**Visual Studio 2022 Community** (visualstudio.microsoft.com):
орнатқышта **Desktop development with C++** workload, ішінде
**C++ CMake tools for Windows** белгіленсін. Жүктеу ~8 ГБ.

Жоба жолында **кириллица болмасын**, OneDrive ішінде болмасын:

```
mkdir C:\dev
cd C:\dev
git clone <сілтеме>
```

VS-те: **File → Open → Folder** → `C:\dev\cg-course` → F5.

Командалық жолмен істесең, «x64 Native Tools Command Prompt for VS 2022»
терминалын аш (кәдімгі PowerShell-де компилятор табылмайды):

```
cmake -B build
cmake --build build --config Debug
build\bin\Debug\cg.exe
```

---

## 2. Linux / WSL2

```bash
sudo apt update
sudo apt install -y build-essential cmake git \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev
```

WSL2 болса, алдымен GPU бар-жоғын тексер:

```bash
sudo apt install -y mesa-utils
glxinfo | grep "OpenGL renderer"
```

`D3D12 (NVIDIA ...)` шықса — дұрыс. `llvmpipe` шықса — GPU үдетуі жоқ,
Visual Studio нұсқасына ауыс.

`MESA: error: ZINK` деген жол қате емес, елеме.

**Жобаны `~/dev` ішінде ұста**, `/mnt/c/...` ішінде емес — әйтпесе
құрастыру бірнеше есе баяу.

---

## 3. macOS

```bash
xcode-select --install
brew install cmake git
```

---

## 4. Тексеру

```bash
cmake -B build
cmake --build build
./build/bin/cg
```

Консольде OpenGL нұсқасы мен видеокарта аты шығуы керек.
`llvmpipe`, `GDI Generic` немесе `Software Rasterizer` жазылса —
драйверді өндірушінің сайтынан жаңарт.

---

## 5. Жиі кездесетін қателер

| Қате | Шешім |
|---|---|
| `No CMAKE_CXX_COMPILER could be found` | VS Installer → Modify → Desktop development with C++ |
| `cmake: command not found` | «x64 Native Tools Command Prompt» терминалын қолдан |
| `Failed to find wayland-scanner` | `rm -rf build`, қайта конфигурациялa |
| `Could NOT find OpenGL` | Linux: `libgl1-mesa-dev` жоқ |
| `fatal error: X11/Xlib.h` | Linux: X11 dev пакеттері жоқ |
| `glad/gl.h: No such file` | 0-бөлімді қара |
| `stb_image.h: No such file` | 0-бөлімді қара |
| Өзгеріс енгізілді, қате қайталанады | **`rm -rf build`** — CMake кэші |
| Құрастыру өте баяу (Windows) | Defender-де `C:\dev` каталогын алып тастау тізіміне қос |

---

## 6. Интернет шектелген болса

`FetchContent` GitHub-қа кіре алмаса, GLFW мен GLM архивтерін алдын ала
`external/glfw` және `external/glm` каталогтарына ашып, `CMakeLists.txt`
ішіндегі `FetchContent` блоктарын мынаған ауыстыр:

```cmake
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
```
