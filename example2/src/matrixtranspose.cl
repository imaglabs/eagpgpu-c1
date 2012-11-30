// transposeMatrix calcula la transpuesta de una matriz de n * n:
// B = transpose(A)

__kernel void transpose(
    __global float* A,
    __global float* B,
    int n)
{
    // Obtener posicion global del thread en la matriz
    const int i= get_global_id(1); // "Coordenada Y del NDRange" (fila i)
    const int j= get_global_id(0); // "Coordenada X del NDRange" (columna j)

    // Verificar que estamos en el rango adecuado
    if(i>=n || j>=n)
        return;

    // Realizar la copia del elemento (i,j) a la posicion (j,i)
    const int index= i * n + j;
    const int transposeIndex= j * n + i;
    B[index]= A[transposeIndex]; // WARNING: lectura coalesciente, escritura no coalesciente
}

/*********************************************************************/

// transposeMatrixShMem calcula la transpuesta de una matriz de n * n:
// B = transpose(A)
// Esta implementacion utiliza la "Shared Memory" para logar accesos "coalescientes" a memoria global
// Con el keyword __attribute__ impongo tamaño de work-group para ejecutar el kernel (BLOCKSIZE x BLOCKSIZE).

#define BLOCKSIZE 16

__kernel __attribute__(( reqd_work_group_size(BLOCKSIZE, BLOCKSIZE, 1) ))
void transposeShMem(
    __global float* A,
    __global float* B,
    int n,
    __local float* block)
{
    // Obtener posicion global del thread en la matriz
    const int in_i= get_global_id(1); // "Coordenada Y del NDRange" (fila i)
    const int in_j= get_global_id(0); // "Coordenada X del NDRange" (columna j)

    // Verificar que estamos en el rango adecuado
    if(in_i>=n || in_j>=n)
        return;
	
    // (1) Cargo bloque a la Shared Memory (lectura coalesciente)
    const int local_i= get_local_id(1);
    const int local_j= get_local_id(0);
    const int indexSharedMem= local_j * BLOCKSIZE + local_i;
    const int indexGlobal= in_i * n + in_j;
    block[indexSharedMem]= A[indexGlobal];
    
    // (2) Barrera para sincronizar los threads del bloque,
    // de manera de asegurarme que todos los threads ya escribieron la memoria compartida
    barrier(CLK_LOCAL_MEM_FENCE);

    // (3) Realizar la escritura a la matriz de salida, leyendo de memoria compartida (escritura coalesciente)
    const int out_j= get_group_id(1) * BLOCKSIZE + local_j;
    const int out_i= get_group_id(0) * BLOCKSIZE + local_i;
    B[out_i * n + out_j]= block[local_i * BLOCKSIZE + local_j];
}
