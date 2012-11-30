float3 forceVector(float3 position) 
{
    const float x = position.x;
    const float y = position.y;
    const float z = position.z;
    
    float Fx, Fy, Fz;

    Fx = -x;
    Fy = 0.0f; 
    Fz = -z; 

    return (float3)(Fx, Fy, Fz);
}

#define MAX_VEL 5.0f

__kernel void vboproc(__global float8* vbo, 
		    int numberOfVertexs,
		    float3 cubeLimits,
		    float dt)
{
    unsigned int index = get_global_id(0);
    
    // chequeo limite
    if (index >= numberOfVertexs)
	return;
    
    // (1) read VBO

    float8 data = vbo[index];
    
    float3 position = data.s012;
    float mass = data.s3;
    float3 velocity = data.s456;
     
    // (2) process data

    float3 force = forceVector(position);
    float3 acceleration = force / mass;
    velocity += acceleration * dt;
    position += velocity * dt;

    velocity = clamp(velocity, (float3)(-MAX_VEL), (float3)(MAX_VEL));
    position = clamp(position, -cubeLimits, cubeLimits);
/*
    if (fabs(position.x) >= cubeLimits.x || fabs(position.y) >= cubeLimits.y || fabs(position.y) >= cubeLimits.z)
	velocity = (float3)(0.0, 0.0, 0.0);
*/
    
    // (3) update VBO
    vbo[index]= (float8)(position, mass, velocity, 0.0f);
    
}

