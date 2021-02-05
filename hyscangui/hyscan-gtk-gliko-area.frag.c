#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2DArray data1;
uniform sampler2DArray data2;
uniform sampler1DArray beam1;
uniform sampler1DArray beam2;
uniform sampler1DArray fade;
uniform float contrast;
uniform float bright;
uniform float rotate;
uniform int tna;
uniform int tnd;
uniform vec4 background;
uniform vec4 colorr;
uniform vec4 colorg;
//uniform float noise;

#define PIx2  6.283185307
#define PI2   1.570796327

void main()
{
  vec2 p, q = vec2( 0.0, 0.0 );
  float r, g, x, y, z1, z2;
  vec4 c;

  p.x = TexCoord.x * 2.0 - 1.0;
  p.y = TexCoord.y * 2.0 - 1.0;

  q.x = length( p );
  if( q.x >= 1.0 )
    discard;
  if( abs( p.y ) > abs( p.x ) )
  {
    q.y = 2.0 * atan( p.x / (q.x + p.y) );
  }
  else if( abs( p.x ) > abs( p.y ) )
  {
    q.y = PI2 - 2.0 * atan( p.y / (q.x + p.x) );
  }
  else
  {
    q.y = 0.0;
  }
  q.y = mod( (q.y + rotate) / PIx2, 1.0 ) * tna;
  q.x = q.x * tnd;
  y = mod( q.y, 1.0 );
  z2 = floor( q.y );
  z1 = z2 * tnd + floor( q.x );
  x = mod( q.x, 1.0 );

  //q.x += fract(sin(dot(q ,vec2(12.9898,78.233))) * 43758.5453) * (1.0 - q.x) * diskreet;
  //q.y += (-noise + 2.0 * noise * fract(sin(dot(q ,vec2(12.9898,78.233))) * 43758.5453) * (1.0 - q.y));

  r = bright + contrast * texture( data1, vec3( x, y, z1 ) ).r;
  r *= texture( fade, vec2( y, z2 ) ).r;
  r += texture( beam1, vec2( y, z2 ) ).r;

  g = bright + contrast * texture( data2, vec3( x, y, z1 ) ).r;
  g *= texture( fade, vec2( y, z2 ) ).r;
  g += texture( beam2, vec2( y, z2 ) ).r;

  // mix(x,y,a) = x*(1-a) + y*a

  //x = (1 + r - g) * (1 + r + g);
  if( r > 0.5 * g )
  {
    c = colorr;
    x = r;
  }
  else if( g > 0.5 * r )
  {
    c = colorg;
    x = g;
  }
  else
  {
    c = mix( colorr, colorg, 0.5 );
    x = max( r, g );
  }
  FragColor = background + c * clamp( x, 0.0, 1.0 - background.a );

  //FragColor = background + colorr * clamp( r, 0.0, 1.0 - background.a );
}
