#include <iostream>
#include <math.h>
#include <cstdlib>
#include <ctime>
#include <memory>

#include "GL/glew.h"
#include "gl/wglew.h"
#include "GL/freeglut.h"

namespace
{
	const size_t MAX_BUFFER_COUNT = 3;
	
	bool gParamUseMapBuffer = false;
	int gParamMaxAllowedTime = 0;
	bool gParamOrphan = true;
	bool gParamDebugMode = false;
	size_t gParamTriangleCount = 100;
	bool gParamShowOnlyResults = false;

	struct SVertex2D
	{
		float x;
		float y;
	};

	const GLchar* gVertexShaderSource[] = {
		"#version 420 core\n"
		"layout (location = 0 ) in vec2 position;\n"
		"void main(void)\n"
		"{\n"
		"  gl_Position = vec4(position,0.0,1.0);\n"
		"}\n"
	};

	const GLchar* gFragmentShaderSource[] = {
		"#version 420 core\n"
		"out vec3 color;\n"
		"void main(void)\n"
		"{\n"
		"  float lerpValue = gl_FragCoord.y / 500.0f;"
		"  color = mix(vec3(0.0,1.0,0.0), vec3(0.0, 0.0, 1.0), lerpValue);\n"
		"}\n"
	};

	std::unique_ptr<SVertex2D[]> gReferenceTrianglePosition = nullptr;
	GLfloat gAngle = 0.0f;

	GLuint gVertexBuffer(0);
	SVertex2D* gVertexBufferData = nullptr;
	GLuint gProgram(0);
	GLuint gFrameCount = 0;
	GLuint gDefaultVao = 0;
}//Unnamed namespace

void logShaderInfo(GLuint shaderID)
{
	int infologLength = 0;
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &infologLength);

	if (infologLength > 0)
	{
		int charsWritten = 0;
		char *infoLog;
		infoLog = (char *)malloc(infologLength);
		glGetShaderInfoLog(shaderID, infologLength, &charsWritten, infoLog);

		printf(infoLog);

		free(infoLog);
	}
}

GLuint CompileShader(const GLchar** shaderSource, GLenum type)
{
	GLuint shader(glCreateShader(type));
	glShaderSource(shader, 1, shaderSource, NULL);
	glCompileShader(shader);

	int compileStatus = GL_TRUE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);

	if (compileStatus == GL_FALSE)
	{
		//printf("Compilation for vertex shader shader failed!");
		logShaderInfo(shader);
		return 0;
	}
	return shader;
}

GLuint CompileShaders(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource)
{
	GLuint vertexShader = CompileShader(vertexShaderSource, GL_VERTEX_SHADER);
	GLuint fragmentShader = CompileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);

	//Link vertex and fragment shader together
	GLuint program(glCreateProgram());
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	//Delete shaders objects
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return program;
}

void PrepareReferenceTriangles(SVertex2D *trianglePos, size_t triangleCount)
{
	srand(static_cast<unsigned int>(time(NULL)));

	for (size_t i = 0; i < triangleCount; ++i)
	{
		float px = ((float)rand() / (float)RAND_MAX)*1.9f - 0.95f;
		float py = ((float)rand() / (float)RAND_MAX)*1.9f - 0.95f;
		float pd = 0.02f;// ((float)rand() / (float)RAND_MAX)*0.02f + 0.01f;

		trianglePos[i * 3 + 0].x = px;
		trianglePos[i * 3 + 0].y = py + pd;
		trianglePos[i * 3 + 1].x = px - pd;
		trianglePos[i * 3 + 1].y = py - pd;
		trianglePos[i * 3 + 2].x = px + pd;
		trianglePos[i * 3 + 2].y = py - pd;
	}
}

void APIENTRY DebugFunc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const GLvoid* userParam)
{
	std::string srcName;
	switch (source)
	{
	case GL_DEBUG_SOURCE_API: srcName = "API"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: srcName = "Window System"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: srcName = "Shader Compiler"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY: srcName = "Third Party"; break;
	case GL_DEBUG_SOURCE_APPLICATION: srcName = "Application"; break;
	case GL_DEBUG_SOURCE_OTHER: srcName = "Other"; break;
	}

	std::string errorType;
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR: errorType = "Error"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: errorType = "Deprecated Functionality"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: errorType = "Undefined Behavior"; break;
	case GL_DEBUG_TYPE_PORTABILITY: errorType = "Portability"; break;
	case GL_DEBUG_TYPE_PERFORMANCE: errorType = "Performance"; break;
	case GL_DEBUG_TYPE_OTHER: errorType = "Other"; break;
	}

	std::string typeSeverity("default");
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH: typeSeverity = "High"; break;
	case GL_DEBUG_SEVERITY_MEDIUM: typeSeverity = "Medium"; break;
	case GL_DEBUG_SEVERITY_LOW: typeSeverity = "Low"; break;
	}

	printf("\nGL Debug Message \"%s\" from \"%s\",\t%s priority \nMessage: %s\n", errorType.c_str(), srcName.c_str(), typeSeverity.c_str(), message);
}

void Init(void)
{
	//Init glew
	glewInit();

	if (!gParamShowOnlyResults)
	{
		printf("OpenGL via: %s\n", glGetString(GL_VENDOR));
		printf("OpenGL ver: %s\n", glGetString(GL_VERSION));
	}

#ifdef WIN32	// make sure vsync is turned off
	if (WGLEW_EXT_swap_control)
		wglSwapIntervalEXT(0);
#endif

	if (gParamDebugMode && GLEW_ARB_debug_output)
	{
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE);
		glDebugMessageCallback((GLDEBUGPROCARB)DebugFunc, nullptr);
		printf("Debug Message Callback turned on!\n");
	}

	//Set clear color
	glClearColor(0.1f, 0.1f, 0.1f, 0.0f);

	//Create and bind the shader program
	gProgram = CompileShaders(gVertexShaderSource, gFragmentShaderSource);
	glUseProgram(gProgram);

	glGenVertexArrays(1, &gDefaultVao);
	glBindVertexArray(gDefaultVao);

	glEnableVertexAttribArray(0);

	//Create a vertex buffer object
	glGenBuffers(1, &gVertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, gVertexBuffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	gReferenceTrianglePosition.reset(new SVertex2D[gParamTriangleCount * 3]);
	PrepareReferenceTriangles(gReferenceTrianglePosition.get(), gParamTriangleCount);

	//Create an immutable data store for the buffer
	const size_t bufferSize( gParamTriangleCount * 3 * sizeof(SVertex2D));

	glBufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, GL_STREAM_DRAW);

	if (!gParamUseMapBuffer)
		gVertexBufferData = new SVertex2D[gParamTriangleCount * 3]; 
}

void Quit()
{
	//Clean-up
	glUseProgram(0);
	glDeleteProgram(gProgram);
	glDeleteBuffers(1, &gVertexBuffer);
	glDeleteVertexArrays(1, &gDefaultVao);

	double perFrame = 1000.0 / ((double)gFrameCount / 5.0);

	std::cout << "Frame counter: " << gFrameCount << " Per frame: " << perFrame << " milisec " << std::endl;

	//Exit application
	exit(0);
}

void Display()
{
	glClear(GL_COLOR_BUFFER_BIT);
	gAngle += 0.001f;

	const size_t bufferSize( gParamTriangleCount * 3 * sizeof(SVertex2D) );

	if (gParamUseMapBuffer)
	{
		if (gParamOrphan)
			gVertexBufferData = (SVertex2D*)glMapBufferRange(GL_ARRAY_BUFFER, 0, bufferSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		else
			gVertexBufferData = (SVertex2D*)glMapBufferRange(GL_ARRAY_BUFFER, 0, bufferSize, GL_MAP_WRITE_BIT);
	}

	const int MAX_ITERS = 1;
	float tx, ty, an;
	for (int iter = 0; iter < MAX_ITERS; ++iter)
	{
		for (size_t i(0); i != gParamTriangleCount * 3; ++i)
		{
			tx = gReferenceTrianglePosition[(i / 3) * 3].x + 0.01f;
			ty = gReferenceTrianglePosition[(i / 3) * 3].y + 0.01f;
			an = gAngle*sinf(tx*3.0f) + ty;
			gVertexBufferData[i].x = (gReferenceTrianglePosition[i].x - tx) * cosf(an) - (gReferenceTrianglePosition[i].y - ty) * sinf(an) + tx;
			gVertexBufferData[i].y = (gReferenceTrianglePosition[i].x - tx) * sinf(an) + (gReferenceTrianglePosition[i].y - ty) * cosf(an) + ty;
		}
	}
	
	if (gParamUseMapBuffer)
		glUnmapBuffer(GL_ARRAY_BUFFER);
	else
	{
		if (gParamOrphan)
		{
			glBufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
			glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSize, gVertexBufferData);
		}
		else
		{
			glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSize, gVertexBufferData);
		}
	}

	//Draw using the vertex buffer
	for (int iter = 0; iter < MAX_ITERS; ++iter)
		glDrawArrays(GL_TRIANGLES, 0, gParamTriangleCount * 3);

	glutSwapBuffers();
	gFrameCount++;

	if (gParamMaxAllowedTime > 0 && glutGet(GLUT_ELAPSED_TIME) > gParamMaxAllowedTime)
		Quit();
}

void OnKeyPress(unsigned char key, int x, int y)
{
	if (key == 27)
		Quit();
}

const char *searchArgv(int argc, char **argv, const char *key, const char *defaultValue)
{
	for (int i = 1; i < argc; ++i)
	{
		char *p = strstr(argv[i], key);
		if (p)
			return p + strlen(key);
	}
	return defaultValue;
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);

	if (argc > 1)
	{
		gParamUseMapBuffer = strstr(searchArgv(argc, argv, "usemap=", ""), "true") != nullptr;
		gParamOrphan = strstr(searchArgv(argc, argv, "orphan=", ""), "true") != nullptr;
		gParamShowOnlyResults = strstr(searchArgv(argc, argv, "resonly=", ""), "true") != nullptr;

		gParamDebugMode = strstr(searchArgv(argc, argv, "debug=", ""), "true") != nullptr;

		const char *strTimeAllowed = searchArgv(argc, argv, "time=", "0");
		gParamMaxAllowedTime = 1000 * atoi(strTimeAllowed);

		const char *strTris = searchArgv(argc, argv, "tris=", "100");
		gParamTriangleCount = atoi(strTris);
	}

	if (!gParamShowOnlyResults)
	{
		std::cout << "GLSamples: Standard Mapped Buffers" << std::endl;
		std::cout << (gParamUseMapBuffer ? "Using glMap*" : "Using glBuffer*Data") << std::endl;
		std::cout << (gParamOrphan ? "Orphaning" : "No orphaning") << std::endl;
		std::cout << "Triangles:    " << gParamTriangleCount << std::endl;
		if (gParamMaxAllowedTime > 0)
			std::cout << "Quit after:   " << gParamMaxAllowedTime / 1000 << " sec" << std::endl;
	}

	if (gParamDebugMode)
		glutInitContextFlags(GLUT_DEBUG);
	glutInitContextProfile(GLUT_CORE_PROFILE);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(500, 500);
	glutCreateWindow("Persistent-mapped buffers example");
	glutIdleFunc(Display);
	glutKeyboardFunc(OnKeyPress);

	Init();

	glutMainLoop();
}


