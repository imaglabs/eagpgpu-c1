uniform highp mat4 matrix;

attribute highp vec3 vertex;
attribute highp vec3 color;

varying highp vec3 vColor;

void main()
{
    gl_Position = matrix * vec4(vertex, 1.0);
    vColor = color;
}
