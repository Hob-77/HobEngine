#include "math/FrustumCuller.h"
#include "core/Logger.h"

namespace Engine
{
    void FrustumCuller::extractFromMatrix(const mat4& vp)
    {
        // Extract frustum planes from GLM column-major ViewProjection matrix
        // Based on: http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
        // and: https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf

        // GLM is column-major: vp[col][row]
        // We need row vectors for plane extraction, so we access:
        // row0 = (vp[0][0], vp[1][0], vp[2][0], vp[3][0])
        // row1 = (vp[0][1], vp[1][1], vp[2][1], vp[3][1])
        // row2 = (vp[0][2], vp[1][2], vp[2][2], vp[3][2])
        // row3 = (vp[0][3], vp[1][3], vp[2][3], vp[3][3])

        vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
        vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
        vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
        vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

        // Extract planes (Gribb-Hartmann method)
        m_planes[LEFT] = row3 + row0;  // Left:   row3 + row0
        m_planes[RIGHT] = row3 - row0;  // Right:  row3 - row0
        m_planes[BOTTOM] = row3 + row1;  // Bottom: row3 + row1
        m_planes[TOP] = row3 - row1;  // Top:    row3 - row1
        m_planes[NEAR] = row3 + row2;  // Near:   row3 + row2
        m_planes[FAR] = row3 - row2;  // Far:    row3 - row2

        // Normalize all planes (plane normal must be unit length)
        for (int i = 0; i < 6; i++)
        {
            vec3 normal = vec3(m_planes[i]);
            float len = length(normal);
            m_planes[i] /= len;
        }

        /*
        LOG_TRACE("Frustum planes extracted and normalized (GLM column-major)");
        */
    }

    bool FrustumCuller::testSphere(const vec3& center, float radius) const
    {
        for (int i = 0; i < 6; i++)
        {
            vec3 normal = vec3(m_planes[i]);
            float d = m_planes[i].w;

            float dist = dot(normal, center) + d;

            if (dist < -radius)
            {
                return false;
            }
        }

        return true;
    }
}