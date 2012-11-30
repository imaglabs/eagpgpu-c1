
__kernel void globCounter(
    __global int* data,
    uint size,
    int value,
    __global uint* globalCounter)
{
    // obtengo indice asociado a este thread
    int index= get_global_id(0);
    
    // Verificar que estamos en el rango adecuado
    if(index>=size)
        return;

    // verifico si el elemento en la posicion index es igual a 'value'
    // y actualizo el contador de memoria global
    if (data[index] == value) {
	atomic_inc(globalCounter);
    }		
}

__kernel void shMemCounter(
    __global int* data,
    uint size,
    int value,
    __local uint* localCounter,
    __global uint* globalCounter)
{
    // obtengo indice asociado a este thread
    int index= get_global_id(0);
    
    // Verificar que estamos en el rango adecuado
    if(index>=size)
        return;

    // Primer thread del bloque inicializa el valor de shared memory
    if (!get_local_id(0)) {
	*localCounter = 0;
    }

    // sincronizo los threads dentro del block para que vean la escritura a 0 de "localCounter"
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // verifico si el elemento en la posicion index es igual a 'value'
    // y actualizo el contador de memoria local
    if (data[index] == value) {
	atomic_inc(localCounter); 
    }		

    // sincronizo los threads dentro del block para que vean las actualizaciones a "localCounter"
    barrier(CLK_LOCAL_MEM_FENCE);

    // Primer thread del bloque hace la escritura final
    if (!get_local_id(0)) {
	atomic_add(globalCounter, *localCounter);
    }

}
