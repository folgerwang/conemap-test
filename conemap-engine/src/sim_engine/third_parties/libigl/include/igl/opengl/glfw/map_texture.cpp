#include "map_texture.h"
#include "background_window.h"
#include "../create_shader_program.h"
#include "../bind_vertex_attrib_array.h"

#include "../gl.h"
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>

template <typename DerivedV, typename DerivedF, typename DerivedU>
IGL_INLINE bool igl::opengl::glfw::map_texture(
  const Eigen::MatrixBase<DerivedV> & _V,
  const Eigen::MatrixBase<DerivedF> & _F,
  const Eigen::MatrixBase<DerivedU> & _U,
  const unsigned char * in_data,
  const int w,
  const int h,
  const int nc,
  std::vector<unsigned char> & out_data)
{
  int out_w = w;
  int out_h = h;
  int out_nc = nc;
  return map_texture(_V,_F,_U,in_data,w,h,nc,out_data,out_w,out_h,out_nc);
}


template <typename DerivedV, typename DerivedF, typename DerivedU>
IGL_INLINE bool igl::opengl::glfw::map_texture(
  const Eigen::MatrixBase<DerivedV> & V,
  const Eigen::MatrixBase<DerivedF> & F,
  const Eigen::MatrixBase<DerivedU> & U,
  const unsigned char * in_data,
  const int in_w,
  const int in_h,
  const int in_nc,
  std::vector<unsigned char> & out_data,
  int & out_w,
  int & out_h,
  int & out_nc)
{
  GLenum format = -1;
  switch(in_nc)
  {
    case 1: format = GL_RED; break;
    case 2: format = GL_RG; break;
    case 3: format = GL_RGB; break;
    case 4: format = GL_RGBA; break;
    default: assert(false && "Unsupported number of channels"); break;
  }

  GLFWwindow * window = nullptr;
  igl::opengl::glfw::background_window(window);
  // Compile each shader
  std::string vertex_shader = R"(
#version 400
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord_v;
out vec2 tex_coord_f;
void main()
{
  tex_coord_f = vec2(tex_coord_v.x,1.-tex_coord_v.y);
  gl_Position = vec4(2.*position.x-1.,2.*(1.-position.y)-1., position.z,1.);
}
)"
    ;
  std::string fragment_shader = R"(
#version 400
layout(location = 0) out vec3 color;
uniform sampler2D tex;
in vec2 tex_coord_f;
void main()
{
  color = texture(tex,tex_coord_f).rgb;
}
)";
  GLuint prog_id =
    igl::opengl::create_shader_program(vertex_shader,fragment_shader,{});

  using Scalar = float;
  using MatrixXS3 = Eigen::Matrix<Scalar,Eigen::Dynamic,3,Eigen::RowMajor>;
  MatrixXS3 V_vbo = MatrixXS3::Zero(V.rows(),3);
  V_vbo.leftCols(V.cols()) = V.template cast<Scalar>();
  MatrixXS3 U_vbo = MatrixXS3::Zero(U.rows(),3);
  U_vbo.leftCols(U.cols()) = U.template cast<Scalar>();
  Eigen::Matrix<unsigned, Eigen::Dynamic, 3, Eigen::RowMajor> F_vbo = 
    F.template cast<unsigned>();

  // Generate and attach buffers to vertex array
  glDisable(GL_CULL_FACE);
  GLuint VAO = 0;
  glGenVertexArrays(1,&VAO);
  glBindVertexArray(VAO);
  GLuint ibo,vbo,tbo,tex;
  glGenBuffers(1,&ibo);
  glGenBuffers(1,&vbo);
  glGenBuffers(1,&tbo);
  glGenTextures(1,&tex);


  out_w = in_w;
  out_h = in_h;
  glClearColor(0,0,1,1);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(0,0,out_w,out_h);

  // Prepare framebuffer
  GLuint fb = 0;
  GLuint out_tex;

  glGenFramebuffers(1, &fb);
  glBindFramebuffer(GL_FRAMEBUFFER, fb);
  glGenTextures(1, &out_tex);
  glBindTexture(GL_TEXTURE_2D, out_tex);
  // always use float for internal storage
  glTexImage2D(GL_TEXTURE_2D, 0,format, out_w, out_h, 0,format, GL_FLOAT, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_tex, 0);
  {
    GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);
  }
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    glfwTerminate();
    return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, fb);

  glUseProgram(prog_id);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, format, in_w, in_h, 0, format, GL_UNSIGNED_BYTE, in_data);
  glUniform1i(glGetUniformLocation(prog_id,"tex"), 0);

  igl::opengl::bind_vertex_attrib_array(prog_id,"position", vbo, U_vbo, true);
  igl::opengl::bind_vertex_attrib_array(prog_id,"tex_coord_v", tbo, V_vbo, true);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glBindVertexArray(VAO);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*F_vbo.size(), F_vbo.data(), GL_DYNAMIC_DRAW);
  glDrawElements(GL_TRIANGLES, F.size(), GL_UNSIGNED_INT, 0);

  out_nc = in_nc;
  out_data.resize(out_nc*out_w*out_h);
  glBindTexture(GL_TEXTURE_2D, out_tex);
  glGetTexImage(GL_TEXTURE_2D, 0, format, GL_UNSIGNED_BYTE, out_data.data());
  glfwDestroyWindow(window);
  glfwTerminate();

  return true;
}

#ifdef IGL_STATIC_LIBRARY
// Explicit template instantiation
// generated by autoexplicit.sh
template bool igl::opengl::glfw::map_texture<Eigen::Matrix<double, -1, -1, 1, -1, -1>, Eigen::Matrix<int, -1, 3, 1, -1, 3>, Eigen::Matrix<double, -1, -1, 1, -1, -1> >(Eigen::MatrixBase<Eigen::Matrix<double, -1, -1, 1, -1, -1> > const&, Eigen::MatrixBase<Eigen::Matrix<int, -1, 3, 1, -1, 3> > const&, Eigen::MatrixBase<Eigen::Matrix<double, -1, -1, 1, -1, -1> > const&, unsigned char const*, int, int, int, std::vector<unsigned char, std::allocator<unsigned char> >&);
template bool igl::opengl::glfw::map_texture<Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, -1, 0, -1, -1>, Eigen::Matrix<double, -1, -1, 0, -1, -1> >(Eigen::MatrixBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, Eigen::MatrixBase<Eigen::Matrix<int, -1, -1, 0, -1, -1> > const&, Eigen::MatrixBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, unsigned char const*, int, int, int, std::vector<unsigned char, std::allocator<unsigned char> >&, int&, int&, int&);
#endif
