#ifndef SPHERICALCAMERA_H
#define SPHERICALCAMERA_H

#include <QVector3D>

#define PI 3.14159265358979f

class SphericalCoord
{
public:
    SphericalCoord() {}
    SphericalCoord(float r, float theta, float phi);

    QVector3D pos() const;

    void addTheta(float dtheta);
    void addPhi(float dphi);
    void addR(float dr);

    void setTheta(float theta) { _theta = theta; }
    void setPhi(float phi) { _phi = phi; }
    void setR(float r) { _r = r; }

    float r() const { return _r; }
    float theta() const { return _theta; }
    float phi() const { return _phi; }

private:
    float _r;
    float _theta;
    float _phi;

};

#endif // SPHERICALCAMERA_H
