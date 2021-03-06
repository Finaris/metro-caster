#include "VecUtils.h"

#include "Object3D.h"
#include "quartic.cpp"
#include "Sampler.h"
#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool Sphere::intersect(const Ray &r, float tmin, Hit &h) const {
    // Locate intersection point ( 2 pts )
    const Vector3f &rayOrigin = r.getOrigin();
    const Vector3f &dir = r.getDirection();

    Vector3f origin = rayOrigin - _center;

    float a = dir.absSquared();
    float b = 2 * Vector3f::dot(dir, origin);
    float c = origin.absSquared() - _radius * _radius;

    // No intersection case.
    if (b * b - 4 * a * c < 0) {
        return false;
    }

    float d = sqrt(b * b - 4 * a * c);

    float tplus = (-b + d) / (2.0f * a);
    float tminus = (-b - d) / (2.0f * a);

    // The two intersections are at the camera back.
    if ((tplus < tmin) && (tminus < tmin)) {
        return false;
    }

    // The two intersections are at the camera front.
    float t = 10000;
    if (tminus > tmin) {
        t = tminus;
    }

    // One intersection at the front. one at the back.
    if ((tplus > tmin) && (tminus < tmin)) {
        t = tplus;
    }

    if (t < h.getT()) {
        Vector3f normal = r.pointAtParameter(t) - _center;
        normal = normal.normalized();
        h.set(t, this->material, normal);
        return true;
    }
    // END STARTER
    return false;
}

const Ray Sphere::sample() {
    std::default_random_engine generator(rand());
    std::uniform_real_distribution<float> theta_dist(0., 2 * M_PI);
    std::uniform_real_distribution<float> cosphi_dist(-1., 1.);

    float theta = theta_dist(generator);
    float cosphi = cosphi_dist(generator);
    float factor = sqrt(1 - cosphi * cosphi);

    Vector3f dir(factor * cos(theta),
                 factor * sin(theta),
                 cosphi);

    return Ray{_radius * dir + _center, dir};
}

// Add object to group
void Group::addObject(Object3D *obj) {
    m_members.push_back(obj);
}

// Return number of objects in group
int Group::getGroupSize() const {
    return (int) m_members.size();
}

bool Group::intersect(const Ray &r, float tmin, Hit &h) const {
    bool hit = false;
    for (Object3D *o : m_members) {
        if (o->intersect(r, tmin, h)) {
            hit = true;
        }
    }
    return hit;
}


bool Plane::intersect(const Ray &r, float tmin, Hit &h) const {
    float t = (_d - Vector3f::dot(r.getOrigin(), _normal)) / Vector3f::dot(r.getDirection(), _normal);
    if ((t > tmin) && (t < h.getT())) {
        h.set(t, this->material, _normal);
        return true;
    }
    return false;
}

bool Area::intersect(const Ray &r, float tmin, Hit &h) const {
    // See if the ray intersects the plane containing the rectangle.
    float t = Vector3f::dot(_corner - r.getOrigin(), _normal) / Vector3f::dot(r.getDirection(), _normal);
    Vector3f P0P = _corner - r.pointAtParameter(t);
    if (abs(Vector3f::dot(P0P.normalized(), _normal)) > 0.01) {
        return false;
    }

    // See if the point is in the rectangle.
    Vector3f Q1 = (Vector3f::dot(P0P, _sideOne) / _sideOne.absSquared()) * _sideOne;
    Vector3f Q2 = (Vector3f::dot(P0P, _sideTwo) / _sideTwo.absSquared()) * _sideTwo;
    if (0 <= Q1.abs() && Q1.abs() <= _sideOne.abs() && 0 <= Q2.abs() && Q2.abs() <= _sideTwo.abs()) {
        // Check if t is in bounds.
        if (t <= tmin || t > h.getT()) {
            return false;
        }

        // Set the hit.
        h.set(t, this->material, _normal);
        return true;
    }
    return false;
}

const Ray Area::sample() {
    // First, select a random point on the area.
    // Create generators for the sampling and get random lengths.
    std::default_random_engine generator(rand());
    std::uniform_real_distribution<float> sideOneDist(0., _sideOne.abs());
    std::uniform_real_distribution<float> sideTwoDist(0., _sideTwo.abs());
    float sideOneScale = sideOneDist(generator);
    float sideTwoScale = sideTwoDist(generator);

    // Fetch the random point and use a cosine weighted sampler (first two args don't matter).
    Vector3f source = _corner + sideOneScale * _sideOne.normalized() + sideTwoScale * _sideTwo.normalized();
    Hit cosineWeightedHit;
    cosineWeightedHit.set(0, this->material, _normal);
    Sampler *sampler = new cosineWeightedHemisphere;
    return Ray(source, sampler->sample(Ray(source, _normal), cosineWeightedHit).normalized());
}

bool Triangle::intersect(const Ray &r, float tmin, Hit &h) const {
    Matrix3f A(_v[0] - _v[1], _v[0] - _v[2], r.getDirection(), true);
    Vector3f b = _v[0] - r.getOrigin();
    Vector3f x = A.inverse() * b;
    float beta = x[0];
    float gamma = x[1];
    float t = x[2];

    if ((beta > 0) && (gamma > 0) && (beta + gamma < 1) && (t > tmin) && (t < h.getT())) {
        Vector3f normal = (1 - beta - gamma) * _normals[0] + beta * _normals[1] + gamma * _normals[2];
        normal.normalize();
        h.set(t, this->material, normal);
        return true;
    }

    return false;
}


bool Torus::intersect(const Ray &r, float tmin, Hit &h) const {
    // method adapted from https://github.com/sasamil/Quartic
    // and http://www.cosinekitty.com/raytrace/chapter13_torus.html
    Vector3f ray_dir = r.getDirection();
    Vector3f ray_org = r.getOrigin();

    float sum_d_sqrd = ray_dir.absSquared();
    float e = ray_org.absSquared() - _r * _r - _R * _R;
    float f = Vector3f::dot(ray_org, ray_dir);
    float four_a_sqrd = 4.f * _R * _R;

    double z = sum_d_sqrd * sum_d_sqrd; // x^4
    double a = 4. * sum_d_sqrd * f; // x^3
    double b = 2. * sum_d_sqrd * e + 4. * f * f + four_a_sqrd * ray_dir[1] * ray_dir[1]; // x^2
    double c = 4. * f * e + 2. * four_a_sqrd * ray_dir[1] * ray_org[1];
    double d = e * e - four_a_sqrd * (_r * _r - ray_org[1] * ray_org[1]);

    a /= z;
    b /= z;
    c /= z;
    d /= z;

    std::complex<double> *solutions = solve_quartic(a, b, c, d);

    // find closest solution
    std::complex<double> solution;
    float t_min = h.getT();
    float t_guess;
    float imag_eps = 0.;
    for (int i = 0; i < 4; i++) {
        solution = solutions[i];

        if (abs(solution.imag()) > imag_eps) {
            continue;
        }

        t_guess = (float) solution.real();

        if ((t_guess < t_min) && (t_guess > tmin)) {
            t_min = t_guess;
        }
    }

    // check that it is the closest hit so far
    if (t_min < h.getT()) {
        Vector3f point = r.pointAtParameter(t_min);

        float alpha = _R / point.xz().abs();
        Vector3f normal((1 - alpha) * point[0], point[1], (1 - alpha) * point[2]);
        normal.normalize();

        h.set(t_min, this->material, normal);
        return true;
    }

    return false;
}


bool Transform::intersect(const Ray &r, float tmin, Hit &h) const {
    Ray new_r(VecUtils::transformPoint(_m_inverse, r.getOrigin()),
              VecUtils::transformDirection(_m_inverse, r.getDirection()));

    bool hit = _object->intersect(new_r, tmin, h);
    if (hit) {
        h.normal = VecUtils::transformDirection(_m_inverse.transposed(), h.normal).normalized();
    }
    return hit;
}