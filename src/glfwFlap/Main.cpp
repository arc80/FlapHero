#include <flapGame/Public.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#define WITH_GL_DEBUG_MESSAGES 0

#define GL_CHECK(call) \
    do { \
        gl##call; \
        PLY_ASSERT(glGetError() == GL_NO_ERROR); \
    } while (0)
#define GL_NO_CHECK(call) (gl##call)

using namespace ply;

void error_callback(int error, const char* description) {
    StdErr::text().format("Error: {}\n", description);
}

#if WITH_GL_DEBUG_MESSAGES
void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                            const GLchar* message, const void* userParam) {
    StdErr::createStringWriter() << message;
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        PLY_DEBUG_BREAK();
    }
}
#endif

Float2 getFramebufferSize(GLFWwindow* window) {
    int renderWidth = 0;
    int renderHeight = 0;
    glfwGetWindowSize(window, &renderWidth, &renderHeight);
    return {(float) renderWidth, (float) renderHeight};
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    flap::GameFlow* gf = (flap::GameFlow*) glfwGetWindowUserPointer(window);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_SPACE) {
        if (action == GLFW_PRESS) {
            doInput(gf, getFramebufferSize(window), {240, 320}, true);
        } else if (action == GLFW_RELEASE) {
            doInput(gf, getFramebufferSize(window), {240, 320}, false);
        }
    }
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        flap::reloadAssets();
    }
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        onBackPressed(gf);
    }
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        togglePause(gf);
    }
}

static void mousebutton_callback(GLFWwindow* window, int button, int action, int mods) {
    flap::GameFlow* gf = (flap::GameFlow*) glfwGetWindowUserPointer(window);
    if (button == GLFW_MOUSE_BUTTON_1) {
        double x = 0, y = 0;
        glfwGetCursorPos(window, &x, &y);
        Float2 fbSize = getFramebufferSize(window);
        doInput(gf, fbSize, {(float) x + 0.5f, fbSize.y - 0.5f - (float) y}, action == GLFW_PRESS);
    }
}

//---------------------------------------------------------------------------
//  Main loop
//---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Initialize GLFW
    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        StdErr::text() << "Error: Could not initialize GLFW\n";
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
#if WITH_GL_DEBUG_MESSAGES
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(480, 640, "Flap Hero", NULL, NULL);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(0);

#if WITH_GL_DEBUG_MESSAGES
    // Enable error logging
    GL_CHECK(Enable(GL_DEBUG_OUTPUT));
    GL_CHECK(DebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER, GL_DEBUG_SEVERITY_NOTIFICATION,
                                 0, nullptr, GL_FALSE));
    GL_CHECK(DebugMessageCallback(debugCallback, nullptr));
#endif

    // Create default VAO; needed before validating shaders
    GLuint vao;
    GL_CHECK(GenVertexArrays(1, &vao)); // Never destroyed
    GL_CHECK(BindVertexArray(vao));

    // Init game
    flap::init(NativePath::join(FLAPGAME_REPO_FOLDER, "data"));

    // Create gf
    flap::GameFlow* gf = flap::createGameFlow();
    glfwSetWindowUserPointer(window, gf);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mousebutton_callback);

    // Main loop
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();

        int renderWidth = 0;
        int renderHeight = 0;
        glfwGetFramebufferSize(window, &renderWidth, &renderHeight);

        // Update the gf
        update(gf, (float) (now - lastTime));
        if (renderWidth > 0 && renderHeight > 0) {
            render(gf, {(float) renderWidth, (float) renderHeight}, (float) (now - lastTime));
        }

        // Present framebuffer
        glfwSwapBuffers(window);
        glfwPollEvents();

        lastTime = now;
    }

    // Destroy gf
    destroy(gf);

    // Shutdown game
    flap::shutdown();

    // Shutdown GLFW
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);

    return 0;
}
