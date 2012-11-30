#include "sphericalcoord.h"

#include <QtCore/qmath.h>

SphericalCoord::SphericalCoord(float r, float theta, float phi) :
    _r(r), _theta(theta), _phi(phi)
{

}

QVector3D SphericalCoord::pos() const
{
    return QVector3D(_r * qSin(_theta * PI/180.0f) * qCos(_phi * PI/180.0f),
                     _r * qCos(_theta * PI/180.0f),
                     -_r * qSin(_theta * PI/180.0f) * qSin(_phi * PI/180.0f));
}

void SphericalCoord::addTheta(float dtheta)
{
    _theta += dtheta;
    if (_theta > 179.0f) {
        _theta=179.0f;
    }
    else if (_theta < 1.0f) {
        _theta=1.0f;
    }

}

void SphericalCoord::addPhi(float dphi)
{
    _phi += dphi;
    if (_phi >= 360.0f or _phi <= -360.0f) {
        int s = (_phi > 0) ? 1 : -1;
        _phi = s * fmod(_phi, 360.0f);
    }
}

void SphericalCoord::addR(float dr)
{
    _r += dr;
    if (_r < 0.0f) {
        _r = 0.0f;
    }
}


