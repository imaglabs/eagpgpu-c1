
__kernel void heatBrush(
    __write_only image2d_t system,
    int bx, int by, int size,
    float value)
{
    int x= get_global_id(0);
    int y= get_global_id(1);

    const int width= get_image_width(system);
    const int height= get_image_height(system);

    if(x>=width || y>=height)
        return;

    // Si estamos dentro del brush seteamos el valor

    float dist= sqrt(convert_float((x-bx)*(x-bx) + (y-by)*(y-by)));
    if(dist > size)
        return;

    write_imagef(system, (int2)(x, y), value);
}
