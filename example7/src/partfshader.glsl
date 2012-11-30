#version 120

uniform sampler2D pointSpriteTex;
uniform sampler2D velTex;

varying highp vec4 vColor;

void main()
{
    vec4 velocityColor = vec4(texture2D(velTex, vec2(length(vColor.rgb/3.0f), 0.5f)).rgb, 1.0f);
    vec4 spriteColor = texture2D(pointSpriteTex, gl_TexCoord[0].st);
    gl_FragColor =  velocityColor * spriteColor; 
}
