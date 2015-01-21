#include <iostream>
#include <math.h>

#include "GL/glew.h"
#include "gl/wglew.h"
#include "GL/freeglut.h"

namespace
{
	bool gSyncBuffers = true;
	bool gUseNBuffering = false;
	size_t gBufferCount = 3;
	bool gBenchmarkMode = false;
	int gMaxAllowedTime = 0;

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
                         "  color = vec3(0.0,1.0,0.0);\n"
                         "}\n" 
                         };
                         
                         
  const SVertex2D gTrianglePosition[] = { {-0.5f,-0.5f}, {0.5f,-0.5f}, {0.0f,0.5f} };
  GLfloat gAngle = 0.0f;
  
  GLuint gVertexBuffer(0);  
  SVertex2D* gVertexBufferData(0);
  GLuint gProgram(0);
  Range gSyncRanges[3];	// max buffer count is 3...
  GLuint gRangeIndex = 0;
  GLsync gSync;
  GLuint gWaitCount = 0;
  GLuint gFrameCount = 0;
}//Unnamed namespace


GLuint CompileShaders(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource )
{
  //Compile vertex shader
  GLuint vertexShader( glCreateShader( GL_VERTEX_SHADER ) );
  glShaderSource( vertexShader, 1, vertexShaderSource, NULL );
  glCompileShader( vertexShader );
  
  //Compile fragment shader
  GLuint fragmentShader( glCreateShader( GL_FRAGMENT_SHADER ) );
  glShaderSource( fragmentShader, 1, fragmentShaderSource, NULL );
  glCompileShader( vertexShader );
  
  //Link vertex and fragment shader together
  GLuint program( glCreateProgram() );
  glAttachShader( program, vertexShader );
  glAttachShader( program, fragmentShader );
  glLinkProgram( program );
  
  //Delete shaders objects
  glDeleteShader( vertexShader );
  glDeleteShader( fragmentShader );   

  return program;  
}


void Init(void)
{
  //Check if Opengl version is at least 4.4
  const GLubyte* glVersion( glGetString(GL_VERSION) );
  int major = glVersion[0] - '0';
  int minor = glVersion[2] - '0';  
  if( major < 4 || minor < 4 )
  {
    std::cerr<<"ERROR: Minimum OpenGL version required for this demo is 4.4. Your current version is "<<major<<"."<<minor<<std::endl;
    exit(-1);
  }

  //Init glew
  glewInit(); 
    
#ifdef WIN32
  if (WGLEW_EXT_swap_control)
	  wglSwapIntervalEXT(0);
#endif

  //Set clear color
  glClearColor(0.1f, 0.1f, 0.1f, 0.0f);
  //glDisable(GL_TEXTURE_2D);
  
  //Create and bind the shader program
  gProgram = CompileShaders( gVertexShaderSource, gFragmentShaderSource );
  glUseProgram(gProgram);
  glEnableVertexAttribArray(0);

  //Create a vertex buffer object
  glGenBuffers( 1, &gVertexBuffer );
  glBindBuffer( GL_ARRAY_BUFFER, gVertexBuffer );
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
  
  //Create an immutable data store for the buffer
  size_t bufferSize( sizeof(gTrianglePosition) ) ;  
  if (gUseNBuffering)
  {
	  bufferSize *= gBufferCount;
	  gSyncRanges[0].begin = 0;
	  gSyncRanges[1].begin = 3;
	  gSyncRanges[2].begin = 6;
  }

  GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
  glBufferStorage( GL_ARRAY_BUFFER, bufferSize, 0, flags );
  
  //Map the buffer forever
  gVertexBufferData = (SVertex2D*)glMapBufferRange( GL_ARRAY_BUFFER, 0, bufferSize, flags ); 

}

void Quit()
{
	//Clean-up
	glUseProgram(0);
	glDeleteProgram(gProgram);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glDeleteBuffers(1, &gVertexBuffer);

	std::cout << "wait counter: " << gWaitCount << ", frame counter: " << gFrameCount << std::endl;

	//Exit application
	exit(0);
}

void LockBuffer(GLsync& syncObj)
{
  if( syncObj )
  {
	  glDeleteSync(syncObj);
  }
  syncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void WaitBuffer(GLsync& syncObj)
{
  if (syncObj)
  {
    while( 1 )	
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
  glClear( GL_COLOR_BUFFER_BIT );
  gAngle += 0.001f;
  
  //Wait until the gpu is no longer using the buffer
  if (gSyncBuffers)
  {
	  if (gUseNBuffering)
		  WaitBuffer(gSyncRanges[gRangeIndex].sync);
	  else
		  WaitBuffer(gSync);
  }
  
  //Modify vertex buffer data using the persistent mapped address

  size_t startID = 0;

  if (gUseNBuffering)
	  startID = gSyncRanges[gRangeIndex].begin;

  const int MAX_ITERS = 1;
  for (int iter = 0; iter < MAX_ITERS; ++iter)
  {
	  for (size_t i(0); i != 3; ++i)
	  {
		  gVertexBufferData[i + startID].x = gTrianglePosition[i].x * cosf(gAngle) - gTrianglePosition[i].y * sinf(gAngle);
		  gVertexBufferData[i + startID].y = gTrianglePosition[i].x * sinf(gAngle) + gTrianglePosition[i].y * cosf(gAngle);
	  }
  }

  //Draw using the vertex buffer
  for (int iter = 0; iter < MAX_ITERS; ++iter)
	  glDrawArrays(GL_TRIANGLES, startID, 3);
  
  //Place a fence wich will be removed when the draw command has finished
  if (gSyncBuffers)
  {
	  if (gUseNBuffering)
		  LockBuffer(gSyncRanges[gRangeIndex].sync);
	  else
		  LockBuffer(gSync);
  }

  gRangeIndex = (gRangeIndex + 1) % gBufferCount;

  glutSwapBuffers();
  gFrameCount++;

  if (gBenchmarkMode)
  {
	  int timeSinceStart = glutGet(GLUT_ELAPSED_TIME);
	  if (timeSinceStart > gMaxAllowedTime)
		  Quit();
  }
}

void OnKeyPress( unsigned char key, int x, int y )
{
  //'Esc' key
  if( key == 27 )
    Quit();    
}


int main( int argc, char** argv )
{
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB );
  glutInitWindowSize(400,400);  
  glutCreateWindow("Persistent-mapped buffers example");  
  glutIdleFunc(Display);
  glutKeyboardFunc( OnKeyPress );
  
  gUseNBuffering = false;
  if (argc > 2)
  {
	  if (strcmp(argv[1], "sync") == 0)
	  {
		  gSyncBuffers = true;
		  std::cout << "syncing..." << std::endl;
	  }
	  else if (strcmp(argv[1], "nosync") == 0)
	  {
		  gSyncBuffers = false;
		  std::cout << "not syncing..." << std::endl;
	  }

	  if (strcmp(argv[2], "triple") == 0)
	  {
		  gUseNBuffering = true;
		  gBufferCount = 3;
		  std::cout << "using triple buffering..." << std::endl;
	  }
	  else if (strcmp(argv[2], "double") == 0)
	  {
		  gUseNBuffering = true;
		  gBufferCount = 2;
		  std::cout << "using double buffering..." << std::endl;
	  }

	  if (argc > 3)
	  {
		  gMaxAllowedTime = 1000*atoi(argv[3]);
		  gBenchmarkMode = gMaxAllowedTime > 0;
		  std::cout << "benchmark mode: time: " << gMaxAllowedTime / 1000 << " sec" << std::endl;
	  }
  }

  Init();
  
  //Enter the GLUT event loop
  glutMainLoop();
}