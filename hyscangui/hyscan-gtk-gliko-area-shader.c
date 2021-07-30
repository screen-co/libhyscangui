#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2DArray data1;
uniform sampler2DArray data2;
uniform sampler1DArray beam1;
uniform sampler1DArray beam2;
uniform sampler1DArray fade1;
uniform sampler1DArray fade2;
uniform float contrast;
uniform float bright;
uniform float balance;
uniform float amprange;
uniform float ampoffset;
uniform float gamma;
uniform float rotate;
uniform float bottom;
uniform float distance;
uniform float freq1;
uniform float freq2;
uniform int tna;
uniform int tnd;
uniform vec4 background;
uniform vec4 colorr;
uniform vec4 colorg;
//uniform float noise;

#define PIx2 6.283185307
#define PI2 1.570796327

void
main ()
{
  vec2 p, q = vec2 (0.0, 0.0);
  float r, g, dr, dg, x, y, zr1, zg1, z2;
  vec4 c;

  p.x = TexCoord.x * 2.0 - 1.0;
  p.y = TexCoord.y * 2.0 - 1.0;

  q.x = length (p);
  if (abs (p.y) > abs (p.x))
    {
      q.y = 2.0 * atan (p.x / (q.x + p.y));
    }
  else if (abs (p.x) > abs (p.y))
    {
      q.y = PI2 - 2.0 * atan (p.y / (q.x + p.x));
    }
  else
    {
      q.y = 0.0;
    }
  q.x = sqrt (q.x * q.x + bottom * bottom);

  dr = q.x * freq1;
  if (dr >= distance)
    discard;
  dr = dr * tnd;

  dg = q.x * freq2;
  if (dg >= distance)
    discard;
  dg = dg * tnd;

  q.y = mod ((q.y + rotate) / PIx2, 1.0) * tna;
  y = mod (q.y, 1.0);
  z2 = floor (q.y);

  zr1 = z2 * tnd + floor (dr);
  r = mod (dr, 1.0);

  zg1 = z2 * tnd + floor (dg);
  g = mod (dg, 1.0);

  //q.x += fract(sin(dot(q ,vec2(12.9898,78.233))) * 43758.5453) * (1.0 - q.x) * diskreet;
  //q.y += (-noise + 2.0 * noise * fract(sin(dot(q ,vec2(12.9898,78.233))) * 43758.5453) * (1.0 - q.y));

  // note: mix(x,y,a) = x*(1-a) + y*a

  r = (texture (data1, vec3 (r, y, zr1)).r - ampoffset) * amprange;
  r = pow (r, gamma);
  r = bright + contrast * r;
  r = 2.0 * r * balance;
  c.r = texture (fade1, vec2 (y, z2)).r;
  r = clamp (r * c.r, 0.0, 1.0);

  g = (texture (data2, vec3 (g, y, zg1)).r - ampoffset) * amprange;
  g = pow (g, gamma);
  g = bright + contrast * g;
  g = 2.0 * g * (1.0 - balance);
  c.g = texture (fade2, vec2 (y, z2)).r;
  g = clamp (g * c.g, 0.0, 1.0);

  if (c.r > c.g)
    {
      c = mix (background, colorr, r);
    }
  else
    {
      c = mix (background, colorg, g);
    }
  c = mix (c, colorr, texture (beam1, vec2 (y, z2)).r);
  c = mix (c, colorg, texture (beam2, vec2 (y, z2)).r);
  FragColor = c;
}
