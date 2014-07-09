#include <iostream> //std::cerr
#include <cstdlib>  //rand
 
#include "GL/glew.h"
#include "GL/freeglut.h"
 
namespace
{
  struct SVertex2D
  {
    float x,y;  //Position
    float u,v;  //Uv
  };
 
  const GLchar* gVertexShaderSource[] = {
                  "#version 430 core\n"
                  "layout (location = 0 ) in vec2 position;\n"
                  "layout (location = 1 ) in vec2 texCoord;\n"
                  "out vec2 uv;\n"
                  "void main(void)\n"
                  "{\n"
                  "  gl_Position = vec4(position,0.0,1.0);\n"
                  "  uv = texCoord;\n"
                  "}\n"
                   };
 
  const GLchar* gFragmentShaderSource[] = {
                  "#version 430 core\n"
                  "out vec4 color;\n"
                  "in vec2 uv;\n"
                  "layout (binding=0) uniform sampler2DArray textureArray;\n"
                  "layout (location=1) uniform int layer;\n"
                  "void main(void)\n"
                  "{\n"
                  "  color = texture(textureArray, vec3(uv.x,uv.y,layer) );\n"
                  "}\n"
                   };
 
  const SVertex2D gQuad[] = { {0.0f,0.0f,0.0f,0.0f},
                                                   {0.1f,0.0f,1.0f,0.0f},
                                                   {0.0f,0.1f,0.0f,1.0f},
                                                   {0.1f,0.1f,1.0f,1.0f}
                                    };
 
  const unsigned int gIndex[] = {0,1,2,1,3,2};
 
  GLuint gArrayTexture(0);
  GLuint gVertexBuffer(0);
  GLuint gElementBuffer(0);
  GLuint gProgram(0);
 
}//Unnamed namespace
 
void GenerateGeometry()
{
  //Generate 100 little quads
  SVertex2D vVertex[400];
  int index(0);
  float xOffset(-0.95f);
  float yOffset(-0.95f );
  for( unsigned int i(0); i!=10; ++i )
  {
    for( unsigned int j(0); j!=10; ++j )
    {
        for( unsigned int k(0); k!=4; ++k)
       {
         vVertex[index].x = gQuad[k].x+xOffset;
         vVertex[index].y = gQuad[k].y+yOffset;
         vVertex[index].u = gQuad[k].u;
         vVertex[index].v = gQuad[k].v;
     index++;
      } 
      xOffset += 0.2f;
    }
    yOffset += 0.2f;
    xOffset = -0.95f;
  }
 
  //Create a vertex buffer object
  glGenBuffers( 1, &gVertexBuffer );
  glBindBuffer( GL_ARRAY_BUFFER, gVertexBuffer );
  glBufferData( GL_ARRAY_BUFFER, sizeof(vVertex), vVertex, GL_STATIC_DRAW );
 
  //Specify vertex attributes for the shader
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SVertex2D), (GLvoid*)0 );
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SVertex2D), (GLvoid*)2 );
 
  //Create an element buffer
  glGenBuffers( 1, &gElementBuffer );
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, gElementBuffer );
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(gIndex), gIndex, GL_STATIC_DRAW );
}
 
void GenerateArrayTexture()
{
  //Generate an array texture
  glGenTextures( 1, &gArrayTexture );
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, gArrayTexture);
 
  //Create storage for the texture. (100 layers of 1x1 texels)
  glTexStorage3D( GL_TEXTURE_2D_ARRAY,
                  1,                    //No mipmaps as textures are 1x1
                  GL_RGB8,              //Internal format
                  1, 1,                 //width,height
                  100                   //Number of layers
                );
 
  for( unsigned int i(0); i!=100;++i)
  {
    //Choose a random color for the i-essim image
    GLubyte color[3] = {rand()%255,rand()%255,rand()%255};
 
    //Specify i-essim image
    glTexSubImage3D( GL_TEXTURE_2D_ARRAY,
                     0,                     //Mipmap number
                     0,0,i,                 //xoffset, yoffset, zoffset
                     1,1,1,                 //width, height, depth
                     GL_RGB,                //format
                     GL_UNSIGNED_BYTE,      //type
                     color);                //pointer to data
  }
 
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}
 
GLuint CompileShaders(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource )
{
  //Compile vertex shader
  GLuint vertexShader( glCreateShader( GL_VERTEX_SHADER ) );
  glShaderSource( vertexShader, 1, vertexShaderSource, NULL );
  glCompileShader( vertexShader );
 
  //Compile fragment shader
  GLuint fragmentShader( glCreateShader( GL_FRAGMENT_SHADER ) );
  glShaderSource( fragmentShader, 1, fragmentShaderSource, NULL );
  glCompileShader( fragmentShader );
 
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
  //Check if Opengl version is at least 3.0
  const GLubyte* glVersion( glGetString(GL_VERSION) );
  int major = glVersion[0] - '0';
  int minor = glVersion[2] - '0';
  if( major < 3 || minor < 0 )
  {
    std::cerr<<"ERROR: Minimum OpenGL version required for this demo is 3.0. Your current version is "<<major<<"."<<minor<<std::endl;
    exit(-1);
  }
 
  //Init glew
  glewInit();
 
  //Set clear color
  glClearColor(1.0, 1.0, 1.0, 0.0);
 
  //Create and bind the shader program
  gProgram = CompileShaders( gVertexShaderSource, gFragmentShaderSource );
  glUseProgram(gProgram);
 
  glUniform1i(0,0); //Sampler refers to texture unit 0
  
  GenerateGeometry();
  GenerateArrayTexture();
}
 
void Display()
{
  glClear( GL_COLOR_BUFFER_BIT );
  for( GLint i(0); i!=100; ++i )
  {
     glUniform1i(1, i);
     glDrawElementsBaseVertex( GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, i*4 );
  }
  glutSwapBuffers();
}
 
void Quit()
{
  //Clean-up
  glDeleteProgram(gProgram);
  glDeleteBuffers( 1, &gVertexBuffer );
  glDeleteBuffers( 1, &gElementBuffer );
  glDeleteTextures( 1, &gArrayTexture );
 
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
  glutCreateWindow("Array Texture Example");
  glutIdleFunc(Display);
  glutKeyboardFunc( OnKeyPress );
 
  Init();
 
  //Enter the GLUT event loop
  glutMainLoop();
}
