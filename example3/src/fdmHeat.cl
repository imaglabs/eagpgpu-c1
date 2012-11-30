
__kernel void fdmHeat(
    __read_only image2d_t input,
    __write_only image2d_t output)
{
    // Obtener posicion global del thread en la matriz
    int x= get_global_id(0); // "Coordenada Y del NDRange" (fila i)
    int y= get_global_id(1); // "Coordenada X del NDRange" (columna j)

    // Tamanio del sistema
    const int width= get_image_width(output);
    const int height= get_image_height(output);

    // Verificar que estamos en el rango adecuado
    if(x>=width || y>=height)
        return;

    // Valor que vamos a escribir en la posicion x,y de output
    float value;
    // "Sampler" para leer de las imagenes
    const sampler_t sampler= CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

    // Si estamos en alguna celda del borde simplemente copiamos la
    // temperatura actual, de forma que la temperatura de los bordes
    // sea fija (Condicion de frontera de Dirichlet)
    if(x==0 || y==0 || x==width-1 || y==height-1) {

        value= read_imagef(input, sampler, (int2)(x,y)).x;

    } else {

        // Obtener el valor de las celdas vecinas
        float up   = read_imagef(input, sampler, (int2)(x  , y-1)).x;
        float down = read_imagef(input, sampler, (int2)(x  , y+1)).x;
        float left = read_imagef(input, sampler, (int2)(x-1, y  )).x;
        float right= read_imagef(input, sampler, (int2)(x+1, y  )).x;

        // Calcular nuevo valor
        value= (up + down + left + right) / 4.0f;
    }

    // Escribir resultado
    write_imagef(output, (int2)(x,y), value);
}
