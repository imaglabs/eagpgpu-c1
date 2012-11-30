#ifndef PARTICLE_H 
#define PARTICLE_H

typedef struct {
    // position
    float px;
    float py;
    float pz;
    // mass
    float m;
    // velocity
    float vx;
    float vy;
    float vz;
    // padding bytes
    float pad;
} Particle;

#endif // PARTICLE_H
