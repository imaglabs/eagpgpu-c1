#version 120

uniform highp mat4 matrix;

attribute highp vec4 vertex;
attribute highp vec4 color;

varying highp vec4 vColor;

#define MAX_POINT_SPRITE_SIZE 30.0f

void main()
{

    vColor = color;

    vec4 pos = matrix * vec4(vertex.xyz, 1.0);
    vec3 eyePos = pos.xyz;

    gl_PointSize = max(MAX_POINT_SPRITE_SIZE * (1.0f/length(eyePos)), 1.0f);
    gl_Position = pos;

}
