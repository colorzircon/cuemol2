// -*-Mode: C++;-*-
//
//  Default fragment shader for OpenGL
//

@include "lib_common.glsl"

uniform float frag_alpha;

void main (void)
{
  //gl_FragColor = calcFogAlpha(gl_Color, gl_FogFragCoord, frag_alpha);
  gl_FragColor = vec4(1,0,0,1);
}

