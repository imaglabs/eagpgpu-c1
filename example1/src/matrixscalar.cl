// Multiplica una matriz de n * n por un escalar k:
// B = k * A

__kernel void matrixScalar(
    __global float* A,
    __global float* B,
    float k,
    int n)
{
    // Obtener posicion global del thread en la matriz
    int i= get_global_id(1); // "Coordenada Y del NDRange" (fila i)
    int j= get_global_id(0); // "Coordenada X del NDRange" (columna j)

    // Verificar que estamos en el rango adecuado
    if(i>=n || j>=n)
        return;

    // Realizar la operacion para el elemento (i,j)
    const int index= i * n + j;
    B[index]= k * A[index];
}
