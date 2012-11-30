varying highp vec3 vColor;
uniform highp float alpha;

void main()
{
    gl_FragColor = vec4(vColor, alpha);
}
