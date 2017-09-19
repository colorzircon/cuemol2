// -*-Mode: C++;-*-
//
//  NameLabel2 vertex shader for OpenGL
//

@include "lib_common.glsl"

////////////////////
// Vertex attributes

attribute vec3 a_xyz;

attribute vec2 a_wh;

attribute float a_width;
attribute float a_addr;

////////////////////
// Uniform variables

uniform vec2 u_winsz;

////////////////////
// Varying

varying vec2 v_labpos;

varying float v_width;
varying float v_addr;

void main (void)
{
  //int ind = gl_VertexID%4;

  // Eye-coordinate position of vertex, needed in various calculations
  vec4 ecPosition = gl_ModelViewMatrix * vec4(a_xyz, 1.0);
  //gEcPosition = ecPosition;

  //ecPosition.x += a_wh.x;
  //ecPosition.y += a_wh.y;

  // Do fixed functionality vertex transform
  gl_Position = gl_ProjectionMatrix * ecPosition;

  gl_Position.x += a_wh.x/u_winsz.x;
  gl_Position.y += a_wh.y/u_winsz.y;

  gl_FrontColor=gl_Color;

  gl_FogFragCoord = ffog(ecPosition.z);

  v_labpos.x = a_wh.x;
  v_labpos.y = a_wh.y;

  v_width = (a_width);
  v_addr = (a_addr);
}
