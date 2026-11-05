// =====================================================================
//  КОМПЬЮТЕРЛІК ГРАФИКА — бір файлдық жоба
//
//  1-АПТА:    терезе, фон анимациясы, пробел -> ақ/бастапқы фон, FPS
//  СЕМИНАР 4: uniform, dt, цикл "жаңарту (update)" және "сызу (draw)"
//  5-АПТА:    индекс буфері (EBO), glDrawElements
//  7-АПТА:    координаттар кеңістіктері, MVP (uModel/uView/uProjection)
//  8-АПТА:    тереңдік буфері, back-face culling, Z-fighting
//             - базалық:     depth test + culling, бес куб
//             - 1-тапсырма:  тереңдік тесінсіз / тесімен (D пернесі)
//             - 2-тапсырма:  Z-fighting жасау және жою (2-сахна, N/R)
//             - 3-тапсырма:  culling тексеру, GL_BACK <-> GL_FRONT (C)
//             - қосымша:     glDepthFunc (G), он куб (3-сахна),
//                            wireframe + culling (TAB), жазық төртбұрыш (Q),
//                            тереңдікті көзбен көру (V)
//
//  Басқару:
//      1 / 2 / 3 — бес куб / Z-fighting / он куб
//      D       — тереңдік тесті қосу/өшіру          (1-тапсырма)
//      N       — near: 0.1 <-> 0.0001  (2-сахнада)   (2-тапсырма)
//      R       — far:  1000 <-> 100    (2-сахнада)   (2-тапсырма)
//      C       — glCullFace: GL_BACK <-> GL_FRONT    (3-тапсырма)
//      K       — culling қосу/өшіру
//      G       — glDepthFunc: LESS -> GREATER -> ALWAYS (қосымша)
//      TAB     — wireframe (қосымша)
//      Q       — жазық төртбұрыш (қосымша)
//      V       — түс = gl_FragCoord.z (қосымша)
//      ПРОБЕЛ  — фонды ақ/қара ауыстыру
//      ESC     — шығу
// =====================================================================

#include <glad/gl.h>      // МІНДЕТТІ: glad әрқашан GLFW-дан БҰРЫН
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>

// ---------------------------------------------------------------------
//  Баптаулар
// ---------------------------------------------------------------------
// Терезе өлшемі: aspect осыдан есептеледі, resize кезінде жаңарады (7-апта)
int gWidth = 800, gHeight = 600;

bool whiteBackground = false;

// Сахна: 1 — бес куб, 2 — Z-fighting, 3 — он куб
int sceneMode = 1;

// 1-тапсырма: тереңдік тесті (пернемен уақытша өшіруге болады)
bool depthTest = true;

// 2-тапсырма: Z-fighting сахнасының near/far мәндері
bool tinyNear = true;     // true: near = 0.0001 (жыпылықтайды), false: 0.1
bool bigFar   = true;     // true: far = 1000,  false: far = 100

// 3-тапсырма: culling
bool cullEnabled = true;
bool cullFront   = false; // false: GL_BACK (дұрыс), true: GL_FRONT (тексеру)

// Қосымша
int  depthFuncIdx = 0;    // 0: GL_LESS, 1: GL_GREATER, 2: GL_ALWAYS
bool wireframe    = false;
bool showQuad     = false;
bool showDepth    = false;

float angle = 0.0f;       // жалпы айналу бұрышы (радиан)

// ---------------------------------------------------------------------
//  Шейдерлер
//  uSolid = 1 болса — вершина түсінің орнына бір түс (Z-fighting үшін
//  екі кубтың түсі әртүрлі болуы керек, әйтпесе жыпылықтау көрінбейді).
//  uShowDepth = 1 болса — түс = gl_FragCoord.z (тереңдік, 0..1).
// ---------------------------------------------------------------------
const char* vertexShaderSrc = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vColor;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
    vColor = aColor;
}
)glsl";

const char* fragmentShaderSrc = R"glsl(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

uniform int  uSolid;
uniform vec3 uSolidColor;
uniform int  uShowDepth;

void main() {
    vec3 c = (uSolid == 1) ? uSolidColor : vColor;
    if (uShowDepth == 1) c = vec3(gl_FragCoord.z);   // 0 — жақын, 1 — алыс
    FragColor = vec4(c, 1.0);
}
)glsl";

// ---------------------------------------------------------------------
//  Күйді консольге шығару (скриншот кезінде қай режим екені көрінсін)
// ---------------------------------------------------------------------
const char* depthFuncName() {
    const char* names[] = {"GL_LESS", "GL_GREATER", "GL_ALWAYS"};
    return names[depthFuncIdx];
}

void printState() {
    std::cout << "сахна " << sceneMode
              << " | depth test: " << (depthTest ? "ON" : "OFF")
              << " | glDepthFunc: " << depthFuncName()
              << " | culling: " << (cullEnabled ? (cullFront ? "GL_FRONT" : "GL_BACK") : "OFF");
    if (sceneMode == 2)
        std::cout << " | near = " << (tinyNear ? 0.0001f : 0.1f)
                  << ", far = "  << (bigFar ? 1000 : 100);
    std::cout << "\n";
}

// ---------------------------------------------------------------------
//  Терезе өлшемі өзгергенде: viewport + gWidth/gHeight
// ---------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow*, int width, int height) {
    if (width == 0 || height == 0) return;   // терезе жиналғанда 0-ге бөлмеу
    gWidth  = width;
    gHeight = height;
    glViewport(0, 0, width, height);
}

// ---------------------------------------------------------------------
//  Пернелер. OpenGL күйін (glEnable/glCullFace/...) тек перне басылған
//  сәтте ауыстырамыз — цикл ішінде әр кадрда қайталау керек емес.
// ---------------------------------------------------------------------
void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;

    switch (key) {
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, true); return;
    case GLFW_KEY_SPACE:  whiteBackground = !whiteBackground;     return;
    case GLFW_KEY_TAB:    wireframe = !wireframe;                 return;
    case GLFW_KEY_Q:      showQuad  = !showQuad;                  return;
    case GLFW_KEY_V:      showDepth = !showDepth;                 return;

    case GLFW_KEY_1: sceneMode = 1; break;
    case GLFW_KEY_2: sceneMode = 2; break;
    case GLFW_KEY_3: sceneMode = 3; break;

    case GLFW_KEY_D:   // 1-тапсырма
        depthTest = !depthTest;
        if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        break;
    case GLFW_KEY_N: tinyNear = !tinyNear; break;   // 2-тапсырма
    case GLFW_KEY_R: bigFar   = !bigFar;   break;   // 2-тапсырма
    case GLFW_KEY_C:   // 3-тапсырма
        cullFront = !cullFront;
        glCullFace(cullFront ? GL_FRONT : GL_BACK);
        break;
    case GLFW_KEY_K:
        cullEnabled = !cullEnabled;
        if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        break;
    case GLFW_KEY_G: {  // қосымша: glDepthFunc
        const GLenum funcs[] = {GL_LESS, GL_GREATER, GL_ALWAYS};
        depthFuncIdx = (depthFuncIdx + 1) % 3;
        glDepthFunc(funcs[depthFuncIdx]);
        break;
    }
    default: return;
    }
    printState();
}

// ---------------------------------------------------------------------
//  Шейдер компиляциясы / бағдарлама
// ---------------------------------------------------------------------
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return shader;
}

GLuint createShaderProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// =====================================================================
//  MAIN
// =====================================================================
int main() {
    if (!glfwInit()) {
        std::cerr << "GLFW іске қосылмады\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);   // терезеде 24 биттік тереңдік буфері болсын
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(gWidth, gHeight,
                                          "Компьютерлік графика — 8-апта: тереңдік",
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Терезе жасалмады. Видеокарта OpenGL 3.3-ті "
                     "қолдамауы мүмкін.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSwapInterval(1);

    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cerr << "GLAD жүктелмеді\n";
        glfwTerminate();
        return -1;
    }

    // HiDPI экранда framebuffer терезеден үлкен болуы мүмкін
    glfwGetFramebufferSize(window, &gWidth, &gHeight);
    glViewport(0, 0, gWidth, gHeight);

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GPU:    " << glGetString(GL_RENDERER) << "\n";

    // Тереңдік буфері бар ма? 24 немесе 32 болуы керек, 0 — буфер жоқ
    int depthBits = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH,
        GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits);
    std::cout << "depth bits: " << depthBits << "\n";

    // -----------------------------------------------------------------
    //  8-АПТА: тереңдік тесті және culling — ЦИКЛГЕ ДЕЙІН, бір рет.
    //  1-тапсырма: glEnable(GL_DEPTH_TEST) жолын түсініктемеге алса,
    //  артқы қырлар алда көрінеді (немесе D пернесін бас).
    // -----------------------------------------------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);    // артқы (камераға арқасымен тұрған) қырлар сызылмайды
    glFrontFace(GL_CCW);    // "алдыңғы" = экранда сағат тіліне қарсы жазылған үшбұрыш

    // -----------------------------------------------------------------
    //  Шейдер + uniform орындарын кэштеу (циклге дейін)
    // -----------------------------------------------------------------
    GLuint shaderProgram = createShaderProgram();
    GLint  locModel      = glGetUniformLocation(shaderProgram, "uModel");
    GLint  locView       = glGetUniformLocation(shaderProgram, "uView");
    GLint  locProj       = glGetUniformLocation(shaderProgram, "uProjection");
    GLint  locSolid      = glGetUniformLocation(shaderProgram, "uSolid");
    GLint  locSolidColor = glGetUniformLocation(shaderProgram, "uSolidColor");
    GLint  locShowDepth  = glGetUniformLocation(shaderProgram, "uShowDepth");

    // -----------------------------------------------------------------
    //  Куб: 36 вершина, xyz + rgb. Әр үшбұрыш СЫРТЫНАН қарағанда
    //  сағат тіліне қарсы (CCW) жазылған — culling осыған сүйенеді.
    //
    //  3-тапсырма: 7-аптадағы массивте артқы, оң және үстіңгі қырлар
    //  сағат тілімен (CW) жазылған екен. GL_BACK кезінде олар жоғалды,
    //  GL_FRONT кезінде тек солар көрінді. Түзету: әр үшбұрыштың
    //  вершина ретін кері айналдырдық.
    // -----------------------------------------------------------------
    float vertices[] = {
        // артқы қыр (z = -0.5) — қызыл. ТҮЗЕТІЛДІ (CW -> CCW)
         0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
        // алдыңғы қыр (z = +0.5) — жасыл
        -0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
        // сол қыр (x = -0.5) — көк
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,
        // оң қыр (x = +0.5) — сары. ТҮЗЕТІЛДІ (CW -> CCW)
         0.5f, -0.5f, -0.5f,   1.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 1.0f, 0.0f,
        // астыңғы қыр (y = -0.5) — көгілдір
        -0.5f, -0.5f, -0.5f,   0.0f, 1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   0.0f, 1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 1.0f, 1.0f,
        // үстіңгі қыр (y = +0.5) — күлгін. ТҮЗЕТІЛДІ (CW -> CCW)
         0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 1.0f
    };

    // VAO + VBO. Вершина: 6 float (орны + түсі), stride = 6 * float
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // -----------------------------------------------------------------
    //  БАЗАЛЫҚ: бес кубтың орны
    // -----------------------------------------------------------------
    glm::vec3 positions[] = {
        glm::vec3( 0.0f,  0.0f,  0.0f),
        glm::vec3( 2.0f,  0.5f, -2.0f),
        glm::vec3(-1.8f, -0.3f, -1.0f),
        glm::vec3( 1.2f, -0.8f, -3.5f),
        glm::vec3(-2.2f,  1.0f, -4.0f)
    };

    // ҚОСЫМША: он куб шеңбер бойымен (радиус 3, орталығы z = -3)
    glm::vec3 ringPositions[10];
    for (int i = 0; i < 10; ++i) {
        float a = glm::radians(36.0f * i);
        ringPositions[i] = glm::vec3(3.0f * std::cos(a), 0.0f, -3.0f + 3.0f * std::sin(a));
    }

    printState();

    double lastFrame = glfwGetTime();

    // =================================================================
    //  Негізгі цикл
    // =================================================================
    while (!glfwWindowShouldClose(window)) {
        double currentFrame = glfwGetTime();
        float  dt = (float)(currentFrame - lastFrame);
        lastFrame = currentFrame;
        float  t  = (float)currentFrame;

        // ------------------------- ЖАҢАРТУ ---------------------------
        angle += dt;

        float aspect = (float)gWidth / (float)gHeight;
        float nearZ = 0.1f, farZ = 100.0f;
        glm::vec3 eye(0.0f, 0.0f, 3.0f);

        if (sceneMode == 2) {
            // 2-тапсырма: near өте кіші болса, тереңдік дәлдігінің жартысы
            // near..2*near аралығына (0.0001..0.0002) кетіп қалады
            nearZ = tinyNear ? 0.0001f : 0.1f;
            farZ  = bigFar   ? 1000.0f : 100.0f;
            // Камера баяу тербеледі — жыпылықтау өрнегі өзгеріп тұрады
            eye = glm::vec3(1.2f * std::sin(t * 0.5f), 0.4f * std::sin(t * 0.3f), 3.0f);
        } else if (sceneMode == 3) {
            eye = glm::vec3(0.0f, 3.0f, 6.0f);   // он кубты жоғарыдан көру
        }

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, nearZ, farZ);
        glm::mat4 view = glm::lookAt(eye,
                                     glm::vec3(0.0f, 0.0f, sceneMode == 3 ? -3.0f : 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));

        // --------------------------- СЫЗУ ----------------------------
        if (whiteBackground) glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        else                 glClearColor(0.10f, 0.12f, 0.18f, 1.0f);

        // Түс буферімен бірге ТЕРЕҢДІК буферін де тазалаймыз. Ұмытылса,
        // бірінші кадрдан кейін экран қатып қалады: буферде ескі тереңдіктер
        // тұрады да, жаңа фрагменттер тесттен өтпейді.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        glUseProgram(shaderProgram);
        glUniform1i(locShowDepth, showDepth ? 1 : 0);
        glUniform1i(locSolid, 0);

        // projection мен view барлық объектіге ортақ — кадрда бір рет
        glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));
        glBindVertexArray(vao);

        if (sceneMode == 1) {
            // --- БАЗАЛЫҚ: бес куб, әрқайсысы өз орнында, өз жылдамдығымен ---
            for (int i = 0; i < 5; i++) {
                // model ЦИКЛ ІШІНДЕ жарияланады: әр кубқа бірлік матрицадан
                // басталады. Сыртында болса, түрлендірулер үстемеленіп, кубтар ұшып кетеді.
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, positions[i]);
                model = glm::rotate(model, angle * (0.5f + i * 0.3f),
                                    glm::vec3(0.3f, 1.0f, 0.0f));
                glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }

            // ҚОСЫМША: жазық төртбұрыш — кубтың алдыңғы қырын (6..11 вершиналар)
            // ғана сызамыз. Culling қосулы кезде оның арты көрінбейді.
            if (showQuad) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(0.0f, 1.3f, -1.0f));
                model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.5f));  // қырды өз осінде айналдыру
                glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
                glDrawArrays(GL_TRIANGLES, 6, 6);
            }

        } else if (sceneMode == 2) {
            // --- 2-ТАПСЫРМА: Z-fighting ---
            // Екі куб бетпе-бет: екіншісінің алдыңғы беті біріншісінен
            // небәрі 0.001-ге алда. Екінші куб xy бойынша сәл кішірек —
            // сонда тек алдыңғы беттер таласады (бүйір қырлары ішінде қалады).
            glUniform1i(locSolid, 1);

            glm::mat4 model = glm::mat4(1.0f);
            glUniform3f(locSolidColor, 1.0f, 0.3f, 0.2f);          // қызғылт
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);

            model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.001f));
            model = glm::scale(model, glm::vec3(0.7f, 0.7f, 1.0f));
            glUniform3f(locSolidColor, 0.2f, 0.6f, 1.0f);          // көк
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);

        } else {
            // --- ҚОСЫМША: он куб шеңбер бойымен, әрқайсысы өз жылдамдығымен ---
            for (int i = 0; i < 10; i++) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, ringPositions[i]);
                model = glm::rotate(model, angle * (0.3f + i * 0.2f),
                                    glm::vec3(1.0f, 1.0f, 0.0f));
                glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
