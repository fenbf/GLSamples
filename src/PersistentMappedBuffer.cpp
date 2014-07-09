#include <iostream>
#include <math.h>

#include "GL/glew.h"
#include "GL/freeglut.h"

namespace
{
  struct SVertex2D
  {
    float x;
    float y;
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
  GLsync gSync;
         
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
    
  //Set clear color
  glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
  
  //Create and bind the shader program
  gProgram = CompileShaders( gVertexShaderSource, gFragmentShaderSource );
  glUseProgram(gProgram);
  glEnableVertexAttribArray(0);

  //Create a vertex buffer object
  glGenBuffers( 1, &gVertexBuffer );
  glBindBuffer( GL_ARRAY_BUFFER, gVertexBuffer );
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
  
  //Create an immutable data store for the buffer
  size_t bufferSize( sizeof(gTrianglePosition) );  
  GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
  glBufferStorage( GL_ARRAY_BUFFER, bufferSize, 0, flags );
  
  //Map the buffer for ever
  gVertexBufferData = (SVertex2D*)glMapBufferRange( GL_ARRAY_BUFFER, 0, bufferSize, flags ); 

}

void LockBuffer()
{
  if( gSync )
  {
    glDeleteSync( gSync );	
  }
  gSync = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
}

void WaitBuffer()
{
  if( gSync )
  {
    while( 1 )	
	{
	  GLenum waitReturn = glClientWaitSync( gSync, GL_SYNC_FLUSH_COMMANDS_BIT, 1 );
	  if (waitReturn == GL_ALREADY_SIGNALED || waitReturn == GL_CONDITION_SATISFIED)
	    return;
    }
  }
}


void Display()
{
  glClear( GL_COLOR_BUFFER_BIT );
  gAngle += 0.1f;
  
  //Wait until the gpu is no longer using the buffer
  WaitBuffer();
  
  //Modify vertex buffer data using the persistent mapped address
  for( size_t i(0); i!=6; ++i )
  {
    gVertexBufferData[i].x = gTrianglePosition[i].x * cosf( gAngle ) - gTrianglePosition[i].y * sinf( gAngle );
    gVertexBufferData[i].y = gTrianglePosition[i].x * sinf( gAngle ) + gTrianglePosition[i].y * cosf( gAngle );    
  }  

  //Draw using the vertex buffer
  glDrawArrays( GL_TRIANGLES, 0, 3 );
  
  //Place a fence wich will be removed when the draw command has finished
  LockBuffer();

  glutSwapBuffers();
}

void Quit()
{
  //Clean-up
  glUseProgram(0);
  glDeleteProgram(gProgram);
  glUnmapBuffer( GL_ARRAY_BUFFER );
  glDeleteBuffers( 1, &gVertexBuffer );
  
  //Exit application
  exit(0);
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
  
  Init();
  
  //Enter the GLUT event loop
  glutMainLoop();
}