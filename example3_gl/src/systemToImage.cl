
__kernel void systemToImage(
    __read_only image2d_t system,
    __write_only image2d_t output,
    __constant uchar4* palette)
{
    int x= get_global_id(0); // "Coordenada Y del NDRange" (fila i)
    int y= get_global_id(1); // "Coordenada X del NDRange" (columna j)

    // Tamanio del sistema
    const int width= get_image_width(output);
    const int height= get_image_height(output);

    // Verificar que estamos en el rango adecuado
    if(x>=width || y>=height)
        return;

    const sampler_t sampler= CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
    float value= read_imagef(system, sampler, (int2)(x,y)).x;

    int index= clamp((int)(value * 255.0f), 0, 255);
    float4 color= convert_float4(palette[index]) / 255.0f;

    // Escribir resultado
    write_imagef(output, (int2)(x, y), color);
}
