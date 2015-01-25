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
	
	bool gParamSyncBuffers   = true;
	size_t gParamBufferCount = 1;
	int gParamMaxAllowedTime = 0;
	bool gParamDebugMode = false;
	size_t gParamTriangleCount = 100;
	bool gParamShowOnlyResults = false;

	struct SVertex2D
	{
		float x;
		float y;
	};

	struct Range
	{
		size_t begin = 0;
		GLsync sync = 0;
	};

	const GLchar* gVertexShaderSource[] = {
		"#version 440 core\n"
		"layout (location = 0 ) in vec2 position;\n"
		"void main(void)\n"
		"{\n"
		"  gl_Position = vec4(position,0.0,1.0);\n"
		"}\n"
	};

	const GLchar* gFragmentShaderSource[] = {
		"#version 440 core\n"
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
	SVertex2D* gVertexBufferData(nullptr);
	GLuint gProgram(0);
	Range gSyncRanges[MAX_BUFFER_COUNT];
	GLuint gRangeIndex = 0;
	GLsync gSyncObject;
	GLuint gWaitCount = 0;
	GLuint gFrameCount = 0;
}//Unnamed namespace

GLuint CompileShaders(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource)
{
	//Compile vertex shader
	GLuint vertexShader(glCreateShader(GL_VERTEX_SHADER));
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	//Compile fragment shader
	GLuint fragmentShader(glCreateShader(GL_FRAGMENT_SHADER));
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	glCompileShader(vertexShader);

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

		trianglePos[i * 3 + 0] = { px, py + pd };
		trianglePos[i * 3 + 1] = { px - pd, py - pd };
		trianglePos[i * 3 + 2] = { px + pd, py - pd };
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

	if (!glewIsSupported("GL_ARB_buffer_storage"))
	{
		std::cerr << "ERROR: \"ARB_buffer_storage\" is missing..." << std::endl;
		exit(-1);
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
		printf("Debug Message Callback turned on!");
	}

	//Set clear color
	glClearColor(0.1f, 0.1f, 0.1f, 0.0f);
	//glDisable(GL_TEXTURE_2D);

	//Create and bind the shader program
	gProgram = CompileShaders(gVertexShaderSource, gFragmentShaderSource);
	glUseProgram(gProgram);
	glEnableVertexAttribArray(0);

	//Create a vertex buffer object
	glGenBuffers(1, &gVertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, gVertexBuffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	gReferenceTrianglePosition.reset(new SVertex2D[gParamTriangleCount * 3]);
	PrepareReferenceTriangles(gReferenceTrianglePosition.get(), gParamTriangleCount);

	//Create an immutable data store for the buffer
	size_t bufferSize{ gParamTriangleCount * 3 * sizeof(SVertex2D)};
	if (gParamBufferCount > 1)
	{
		bufferSize *= gParamBufferCount;
		gSyncRanges[0].begin = 0;
		gSyncRanges[1].begin = gParamTriangleCount * 3;
		gSyncRanges[2].begin = gParamTriangleCount * 3 * 2;
	}

	GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	glBufferStorage(GL_ARRAY_BUFFER, bufferSize, 0, flags);

	//Map the buffer forever
	gVertexBufferData = (SVertex2D*)glMapBufferRange(GL_ARRAY_BUFFER, 0, bufferSize, flags);
}

void Quit()
{
	//Clean-up
	glUseProgram(0);
	glDeleteProgram(gProgram);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glDeleteBuffers(1, &gVertexBuffer);

	double perFrame = 1000.0/((double)gFrameCount / 5.0);

	std::cout << "Wait counter: " << gWaitCount << " Frame counter: " << gFrameCount << " Per frame: " << perFrame << " milisec " << std::endl;

	//Exit application
	exit(0);
}

void LockBuffer(GLsync& syncObj)
{
	if (syncObj)
		glDeleteSync(syncObj);

	syncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void WaitBuffer(GLsync& syncObj)
{
	if (syncObj)
	{
		while (1)
		{
			GLenum waitReturn = glClientWaitSync(syncObj, GL_SYNC_FLUSH_COMMANDS_BIT, 1);
			if (waitReturn == GL_ALREADY_SIGNALED || waitReturn == GL_CONDITION_SATISFIED)
				return;

			gWaitCount++;
		}
	}
}

void Display()
{
	glClear(GL_COLOR_BUFFER_BIT);
	gAngle += 0.001f;

	//Wait until the gpu is no longer using the buffer
	if (gParamSyncBuffers)
	{
		if (gParamBufferCount > 1)
			WaitBuffer(gSyncRanges[gRangeIndex].sync);
		else
			WaitBuffer(gSyncObject);
	}

	//Modify vertex buffer data using the persistent mapped address

	size_t startID = 0;

	if (gParamBufferCount > 1)
		startID = gSyncRanges[gRangeIndex].begin;

	const int MAX_ITERS = 1;
	float tx, ty, an;
	for (int iter = 0; iter < MAX_ITERS; ++iter)
	{
		for (size_t i(0); i != gParamTriangleCount * 3; ++i)
		{
			tx = gReferenceTrianglePosition[(i / 3) * 3].x + 0.01f;
			ty = gReferenceTrianglePosition[(i / 3) * 3].y + 0.01f;
			an = gAngle*sinf(tx*3.0f) + ty;
			gVertexBufferData[i + startID].x = (gReferenceTrianglePosition[i].x - tx) * cosf(an) - (gReferenceTrianglePosition[i].y - ty) * sinf(an) + tx;
			gVertexBufferData[i + startID].y = (gReferenceTrianglePosition[i].x - tx) * sinf(an) + (gReferenceTrianglePosition[i].y - ty) * cosf(an) + ty;
		}
	}
	//glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

	//Draw using the vertex buffer
	for (int iter = 0; iter < MAX_ITERS; ++iter)
		glDrawArrays(GL_TRIANGLES, startID, gParamTriangleCount * 3);

	//Place a fence which will be removed when the draw command has finished
	if (gParamSyncBuffers)
	{
		if (gParamBufferCount > 1)
			LockBuffer(gSyncRanges[gRangeIndex].sync);
		else
			LockBuffer(gSyncObject);
	}

	gRangeIndex = (gRangeIndex + 1) % gParamBufferCount;

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
		gParamSyncBuffers = strstr(searchArgv(argc, argv, "sync=", ""), "true") != nullptr;
		gParamDebugMode = strstr(searchArgv(argc, argv, "debug=", ""), "true") != nullptr;
		gParamShowOnlyResults = strstr(searchArgv(argc, argv, "resonly=", ""), "true") != nullptr;

		if (searchArgv(argc, argv, "single", nullptr))
			gParamBufferCount = 1;
		else if (searchArgv(argc, argv, "double", nullptr))
			gParamBufferCount = 2;
		else if (searchArgv(argc, argv, "triple", nullptr))
			gParamBufferCount = 3;

		const char *strTimeAllowed = searchArgv(argc, argv, "time=", "0");
		gParamMaxAllowedTime = 1000 * atoi(strTimeAllowed);

		const char *strTris = searchArgv(argc, argv, "tris=", "100");
		gParamTriangleCount = atoi(strTris);
	}

	if (!gParamShowOnlyResults)
	{
		std::cout << "GLSamples: Persistent Mapped Buffers" << std::endl;
		std::cout << "Triangles:    " << gParamTriangleCount << std::endl;
		std::cout << "Sync:         " << (gParamSyncBuffers ? "on" : "off") << std::endl;
		std::cout << "Buffer count: " << (gParamBufferCount == 1 ? "single" : gParamBufferCount == 2 ? "double" : "triple") << std::endl;
		if (gParamMaxAllowedTime > 0)
			std::cout << "Quit after:   " << gParamMaxAllowedTime / 1000 << " sec" << std::endl;
	}

	//glutInitContextVersion(4, 2);
	glutInitContextFlags(GLUT_FORWARD_COMPATIBLE | GLUT_DEBUG);
	//glutInitContextProfile(GLUT_CORE_PROFILE);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(500, 500);
	glutCreateWindow("Persistent-mapped buffers example");
	glutIdleFunc(Display);
	glutKeyboardFunc(OnKeyPress);

	Init();

	glutMainLoop();
}